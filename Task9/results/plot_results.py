#!/usr/bin/env python3
"""
plot_results.py
───────────────
Читает results.csv и speedup.csv, строит:
  1. Время выполнения (мс) для каждого подхода vs. число потоков
  2. График ускорения (speedup)
  3. График эффективности (efficiency)

Использование:
    python3 plot_results.py
    python3 plot_results.py --results r.csv --speedup s.csv --out ./plots
"""

import argparse
import os
import sys
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker

# ── Стили ─────────────────────────────────────────────────────────────────────

PALETTE = {
    "single":          "#6c757d",
    "block":           "#0d6efd",
    "channel_queue":   "#dc3545",
    "channel_atomic":  "#198754",
    "channel_chunked": "#fd7e14",
}
MARKERS = ["o", "s", "^", "D", "v"]

LABELS = {
    "single":          "Single (baseline)",
    "block":           "Block (статический)",
    "channel_queue":   "Channel Queue (динамический)",
    "channel_atomic":  "Channel + Atomic (динамический)",
    "channel_chunked": "Channel Chunked (динамический)",
}

# ─────────────────────────────────────────────────────────────────────────────

def load_data(results_path, speedup_path):
    for p in (results_path, speedup_path):
        if not os.path.exists(p):
            sys.exit(f"[ERROR] File not found: {p}")
    return pd.read_csv(results_path), pd.read_csv(speedup_path)


def style_ax(ax, title, xlabel, ylabel, thread_counts):
    ax.set_title(title, fontsize=13, fontweight="bold", pad=10)
    ax.set_xlabel(xlabel, fontsize=11)
    ax.set_ylabel(ylabel, fontsize=11)
    ax.set_xticks(thread_counts)
    ax.xaxis.set_minor_locator(mticker.NullLocator())
    ax.grid(axis="y", linestyle="--", alpha=0.5)
    ax.grid(axis="x", linestyle=":", alpha=0.3)
    ax.legend(fontsize=9, framealpha=0.9)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)


def plot_time(results, out_dir):
    fig, ax = plt.subplots(figsize=(8, 5))
    approaches = results["approach"].unique()
    thread_counts = sorted(results["n_threads"].unique())
    for idx, approach in enumerate(approaches):
        df = results[results["approach"] == approach].sort_values("n_threads")
        ax.plot(df["n_threads"], df["time_ms"],
                marker=MARKERS[idx % len(MARKERS)],
                color=PALETTE.get(approach, f"C{idx}"),
                label=LABELS.get(approach, approach),
                linewidth=2, markersize=7)
    style_ax(ax, "Время выполнения", "Число потоков", "Время (мс)", thread_counts)
    fig.tight_layout()
    path = os.path.join(out_dir, "time.png")
    fig.savefig(path, dpi=150); print(f"Saved: {path}"); plt.close(fig)


def plot_speedup(speedup, out_dir):
    fig, ax = plt.subplots(figsize=(8, 5))
    approaches = speedup["approach"].unique()
    thread_counts = sorted(speedup["n_threads"].unique())
    ideal_x = [1] + list(thread_counts)
    ax.plot(ideal_x, ideal_x, linestyle="--", color="black",
            linewidth=1.2, label="Идеальное ускорение", zorder=1)
    for idx, approach in enumerate(approaches):
        df = speedup[speedup["approach"] == approach].sort_values("n_threads")
        ax.plot(df["n_threads"], df["speedup"],
                marker=MARKERS[idx % len(MARKERS)],
                color=PALETTE.get(approach, f"C{idx}"),
                label=LABELS.get(approach, approach),
                linewidth=2, markersize=7)
    style_ax(ax, "Ускорение (Speedup = T_single / T_parallel)",
             "Число потоков", "Ускорение", thread_counts)
    fig.tight_layout()
    path = os.path.join(out_dir, "speedup.png")
    fig.savefig(path, dpi=150); print(f"Saved: {path}"); plt.close(fig)


def plot_efficiency(speedup, out_dir):
    fig, ax = plt.subplots(figsize=(8, 5))
    approaches = speedup["approach"].unique()
    thread_counts = sorted(speedup["n_threads"].unique())
    ax.axhline(1.0, linestyle="--", color="black",
               linewidth=1.2, label="Идеальная эффективность")
    for idx, approach in enumerate(approaches):
        df = speedup[speedup["approach"] == approach].sort_values("n_threads")
        ax.plot(df["n_threads"], df["efficiency"],
                marker=MARKERS[idx % len(MARKERS)],
                color=PALETTE.get(approach, f"C{idx}"),
                label=LABELS.get(approach, approach),
                linewidth=2, markersize=7)
    style_ax(ax, "Эффективность (Efficiency = Speedup / N_threads)",
             "Число потоков", "Эффективность", thread_counts)
    ax.set_ylim(0, 1.25)
    fig.tight_layout()
    path = os.path.join(out_dir, "efficiency.png")
    fig.savefig(path, dpi=150); print(f"Saved: {path}"); plt.close(fig)


def print_table(results, speedup):
    merged = speedup.merge(
        results[["n_threads", "approach", "time_ms"]],
        on=["n_threads", "approach"], how="left"
    )
    print("\n" + "═" * 74)
    print(f"{'Подход':<28} {'Потоки':>7} {'Время,мс':>10} {'Speedup':>9} {'Efficiency':>11}")
    print("─" * 74)
    for approach in merged["approach"].unique():
        df = merged[merged["approach"] == approach].sort_values("n_threads")
        label = LABELS.get(approach, approach)
        for _, row in df.iterrows():
            print(f"{label:<28} {int(row['n_threads']):>7} {int(row['time_ms']):>10} "
                  f"{row['speedup']:>9.3f} {row['efficiency']:>11.3f}")
        print("─" * 74)
    print("═" * 74 + "\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--results", default="../results.csv")
    parser.add_argument("--speedup", default="../speedup.csv")
    parser.add_argument("--out",     default=".")
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)
    results, speedup = load_data(args.results, args.speedup)
    print(f"Loaded {len(results)} result rows, {len(speedup)} speedup rows.")
    print_table(results, speedup)
    plot_time(results, args.out)
    plot_speedup(speedup, args.out)
    plot_efficiency(speedup, args.out)
    print("\nВсе графики сохранены!")

if __name__ == "__main__":
    main()
