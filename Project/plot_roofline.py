#!/usr/bin/env python3
"""
plot_roofline.py — Cache-Aware Roofline plot for VRP delta evaluation.

Reads results.csv produced by delta_eval_bench and draws:
  • Theoretical peak compute ridge (horizontal)
  • Measured + theoretical memory-bandwidth ceilings (diagonal)
  • L2 and L1 cache-bandwidth ceilings (diagonal, lighter)
  • Two measurement points: naive and optimized variants

Saves:  roofline.png  (and roofline.pdf)  in the same directory as the CSV.

Usage:
    python3 plot_roofline.py [results.csv]
    (default: results.csv in the current directory)
"""

import sys
import os
import csv
import math
import numpy as np
import matplotlib
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.lines import Line2D

matplotlib.rcParams['pdf.fonttype'] = 42   # embeddable fonts for LaTeX
matplotlib.rcParams['ps.fonttype']  = 42


# ─────────────────────────────────────────────────────────────────────────────
# 1.  Parse CSV
# ─────────────────────────────────────────────────────────────────────────────

def parse_csv(path: str) -> dict:
    result = {}
    variants = []

    with open(path, newline="") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue

            parts = line.split(",")

            if len(parts) == 2:
                key, val = parts[0].strip(), parts[1].strip()
                try:
                    result[key] = float(val)
                except ValueError:
                    pass
                continue

            if len(parts) == 6 and parts[0].strip() in ("naive", "optimized"):
                name = parts[0].strip()
                try:
                    variants.append({
                        "name":           name,
                        "time_sec":       float(parts[1]),
                        "total_evals":    int(parts[2]),
                        "ai_flop_byte":   float(parts[3]),
                        "gflops":         float(parts[4]),
                        "bw_gbs":         float(parts[5]),
                    })
                except ValueError:
                    pass

    result["variants"] = variants
    return result


# ─────────────────────────────────────────────────────────────────────────────
# 2.  Build Roofline model
# ─────────────────────────────────────────────────────────────────────────────

def roofline_flops(ai, peak_flops, peak_bw):
    """Roofline attainable GFLOP/s for a given arithmetic intensity."""
    return np.minimum(peak_bw * ai, peak_flops)


# ─────────────────────────────────────────────────────────────────────────────
# 3.  Plot
# ─────────────────────────────────────────────────────────────────────────────

COLORS = {
    "naive":     "#E84855",   # vivid red
    "optimized": "#3E92CC",   # steel blue
}

VARIANT_LABELS = {
    "naive":     "Naive  (sqrt × 3 per eval)",
    "optimized": "Optimised  (precomputed matrix)",
}

ANNOTATION_STYLE = dict(
    fontsize=8.5,
    ha="left",
    va="bottom",
    bbox=dict(boxstyle="round,pad=0.25", fc="white", ec="none", alpha=0.75),
)


def plot_roofline(data: dict, out_dir: str) -> None:
    peak_flops   = data.get("hw_peak_flops_gflops", 25.6)
    peak_mem_bw  = data.get("hw_peak_mem_bw_gbs",   68.0)
    meas_mem_bw  = data.get("measured_mem_bw_gbs",  60.0)
    peak_l2_bw   = data.get("hw_peak_l2_bw_gbs",   250.0)
    peak_l1_bw   = data.get("hw_peak_l1_bw_gbs",   800.0)
    variants     = data["variants"]

    ridge_mem  = peak_flops / peak_mem_bw
    ridge_l2   = peak_flops / peak_l2_bw
    ridge_l1   = peak_flops / peak_l1_bw

    ai_min = 1e-3
    ai_max = max(10.0,
                 max(v["ai_flop_byte"] for v in variants) * 3,
                 ridge_mem * 5)
    ai = np.logspace(math.log10(ai_min), math.log10(ai_max), 1000)

    # ── Figure setup ─────────────────────────────────────────────────────────
    fig, ax = plt.subplots(figsize=(9, 5.5))
    ax.set_xscale("log")
    ax.set_yscale("log")

    # ── Cache-bandwidth ceilings (faint) ─────────────────────────────────────
    ax.plot(ai, roofline_flops(ai, peak_flops, peak_l1_bw),
            color="#CCCCCC", lw=1.1, ls="--", zorder=1,
            label=f"L1 bandwidth ceil  ({peak_l1_bw:.0f} GB/s)")
    ax.plot(ai, roofline_flops(ai, peak_flops, peak_l2_bw),
            color="#AAAAAA", lw=1.1, ls="--", zorder=1,
            label=f"L2 bandwidth ceil  ({peak_l2_bw:.0f} GB/s)")



    # ── Theoretical memory bandwidth + peak compute (main Roofline) ──────────
    ax.plot(ai, roofline_flops(ai, peak_flops, peak_mem_bw),
            color="#2D6A4F", lw=2.5, ls="-", zorder=3,
            label=f"Theo. DRAM BW  ({peak_mem_bw:.0f} GB/s)")
    ax.axhline(peak_flops, color="#1B4332", lw=2.5, ls=":", zorder=3,
               label=f"Peak compute  ({peak_flops:.1f} GFLOP/s)")

    # ── Measured memory bandwidth ceiling ─────────────────────────────────────
    ax.plot(ai, roofline_flops(ai, peak_flops, meas_mem_bw),
            color="#F4A261", lw=2.0, ls="--", zorder=2,
            label=f"Measured DRAM BW  ({meas_mem_bw:.1f} GB/s)")

    # Ridge annotation
    ax.annotate(
        f"  Ridge\n  AI={ridge_mem:.2f}",
        xy=(ridge_mem, peak_flops),
        fontsize=7.5, color="#1B4332",
        xytext=(ridge_mem * 1.2, peak_flops * 0.88),
        arrowprops=dict(arrowstyle="-", color="#1B4332", lw=0.8),
    )

    # ── Measurement points ────────────────────────────────────────────────────
    for v in variants:
        name  = v["name"]
        ai_pt = v["ai_flop_byte"]
        gf_pt = v["gflops"]
        color = COLORS[name]
        label = VARIANT_LABELS[name]

        # Attainable GFLOP/s at this AI (for efficiency computation)
        attain = roofline_flops(np.array([ai_pt]), peak_flops, peak_mem_bw)[0]
        eff    = gf_pt / attain * 100.0 if attain > 0 else 0.0

        ax.scatter(ai_pt, gf_pt, color=color, s=110, zorder=5,
                   edgecolors="white", linewidths=1.2, label=label)

        # Vertical dashed line to roof
        roof_at = roofline_flops(np.array([ai_pt]), peak_flops, peak_mem_bw)[0]
        ax.plot([ai_pt, ai_pt], [gf_pt, roof_at],
                color=color, lw=0.9, ls=":", alpha=0.6, zorder=4)

        # Annotation box
        ax.annotate(
            f"  {name.capitalize()}\n"
            f"  AI = {ai_pt:.3f} FLOP/B\n"
            f"  {gf_pt:.3f} GFLOP/s\n"
            f"  η = {eff:.1f}%",
            xy=(ai_pt, gf_pt),
            xytext=(ai_pt * 1.35, gf_pt * (1.6 if name == "naive" else 0.55)),
            color=color,
            **ANNOTATION_STYLE,
            arrowprops=dict(arrowstyle="-", color=color, lw=0.9),
        )

    # ── Axes labels & formatting ──────────────────────────────────────────────
    ax.set_xlabel("Arithmetic Intensity  [FLOP / byte]", fontsize=11)
    ax.set_ylabel("Attainable Performance  [GFLOP/s]", fontsize=11)
    ax.set_title(
        "Cache-Aware Roofline Model — VRP Delta Evaluation\n"
        "Apple M1 Pro  ·  C++17 / -O2 / -march=native",
        fontsize=12, fontweight="bold",
    )

    # x-axis: show FLOP/byte values at integer powers of 2
    ax.set_xlim(ai_min, ai_max)
    ax.set_ylim(1e-3, peak_flops * 3)
    ax.grid(True, which="both", ls=":", alpha=0.35)
    ax.tick_params(axis="both", which="major", labelsize=9)

    # ── Legend ────────────────────────────────────────────────────────────────
    ax.legend(
        loc="upper left",
        fontsize=8,
        framealpha=0.9,
        edgecolor="#DDDDDD",
    )

    # ── Summary table inset ───────────────────────────────────────────────────
    col_labels = ["Variant", "AI\n[FLOP/B]", "GFLOP/s", "BW\n[GB/s]", "Time\n[s]"]
    row_data = [
        [
            v["name"].capitalize(),
            f"{v['ai_flop_byte']:.4f}",
            f"{v['gflops']:.4f}",
            f"{v['bw_gbs']:.3f}",
            f"{v['time_sec']:.2f}",
        ]
        for v in variants
    ]
    table = ax.table(
        cellText=row_data,
        colLabels=col_labels,
        loc="lower right",
        cellLoc="center",
    )
    table.auto_set_font_size(False)
    table.set_fontsize(7.5)
    table.scale(1, 1.35)
    for (row, col), cell in table.get_celld().items():
        cell.set_edgecolor("#CCCCCC")
        if row == 0:
            cell.set_facecolor("#EEF2F6")
            cell.set_text_props(fontweight="bold")
        elif row == 1:
            cell.set_facecolor("#FFF0F0")
        else:
            cell.set_facecolor("#F0F4FF")

    plt.tight_layout()

    # ── Save ──────────────────────────────────────────────────────────────────
    for ext in ("png", "pdf"):
        out_path = os.path.join(out_dir, f"roofline.{ext}")
        fig.savefig(out_path, dpi=180, bbox_inches="tight")
        print(f"Saved: {out_path}")

    plt.close(fig)


# ─────────────────────────────────────────────────────────────────────────────
# 4.  Entry point
# ─────────────────────────────────────────────────────────────────────────────

def main():
    csv_path = sys.argv[1] if len(sys.argv) > 1 else "results.csv"
    if not os.path.isfile(csv_path):
        print(f"ERROR: CSV file not found: {csv_path}", file=sys.stderr)
        sys.exit(1)

    out_dir = os.path.dirname(os.path.abspath(csv_path))
    print(f"Reading: {csv_path}")
    data = parse_csv(csv_path)

    if not data.get("variants"):
        print("ERROR: No variant rows found in CSV.", file=sys.stderr)
        sys.exit(1)

    print(f"Found {len(data['variants'])} variant(s).")
    print(f"Peak compute : {data.get('hw_peak_flops_gflops', '?')} GFLOP/s")
    print(f"Peak BW      : {data.get('hw_peak_mem_bw_gbs', '?')} GB/s")
    print(f"Measured BW  : {data.get('measured_mem_bw_gbs', '?')} GB/s")
    print()

    plot_roofline(data, out_dir)


if __name__ == "__main__":
    main()
