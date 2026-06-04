#!/usr/bin/env python3
"""
Построение графиков метрик сортировок из results.csv
Запуск: python3 plot.py
"""
import csv
import os
from collections import defaultdict

# ── Попытка импортировать matplotlib ────────────────────────────────────────
try:
    import matplotlib.pyplot as plt
    HAS_MPL = True
except ImportError:
    HAS_MPL = False
    print("[WARN] matplotlib not found. Install with: pip install matplotlib")
    print("       Printing table to stdout instead.\n")

# ── Загрузка данных ──────────────────────────────────────────────────────────
data = defaultdict(lambda: {"sizes": [], "time": [], "cmp": [], "swaps": []})

with open("results.csv", newline="") as f:
    reader = csv.DictReader(f)
    for row in reader:
        key = row["algorithm|data_type"]
        data[key]["sizes"].append(int(row["size"]))
        data[key]["time"].append(float(row["time_sec"]))
        data[key]["cmp"].append(int(row["comparisons"]))
        data[key]["swaps"].append(int(row["pointer_swaps"]))

# ── Цветовая схема ───────────────────────────────────────────────────────────
STYLES = {
    "insertion_sort|random":        ("tab:blue",   "o", "--"),
    "insertion_sort|sorted":        ("tab:cyan",   "s", "--"),
    "insertion_sort|reverse":       ("tab:red",    "^", "--"),
    "insertion_sort|nearly_sorted": ("tab:purple", "D", "--"),
    "merge_sort|random":            ("tab:green",  "o", "-"),
    "merge_sort|sorted":            ("tab:olive",  "s", "-"),
    "merge_sort|reverse":           ("tab:orange", "^", "-"),
    "merge_sort|nearly_sorted":     ("tab:brown",  "D", "-"),
}

LABELS = {
    "insertion_sort|random":        "InsSort / random",
    "insertion_sort|sorted":        "InsSort / sorted",
    "insertion_sort|reverse":       "InsSort / reverse",
    "insertion_sort|nearly_sorted": "InsSort / nearly sorted",
    "merge_sort|random":            "MergeSort / random",
    "merge_sort|sorted":            "MergeSort / sorted",
    "merge_sort|reverse":           "MergeSort / reverse",
    "merge_sort|nearly_sorted":     "MergeSort / nearly sorted",
}

# ── Функция построения одного графика ────────────────────────────────────────
def make_plot(metric_key, ylabel, title, filename):
    fig, ax = plt.subplots(figsize=(9, 5))
    for key, vals in data.items():
        color, marker, ls = STYLES.get(key, ("gray", "x", ":"))
        ax.plot(vals["sizes"], vals[metric_key],
                color=color, marker=marker, linestyle=ls,
                label=LABELS.get(key, key), linewidth=1.6, markersize=5)
    ax.set_xlabel("Размер списка (n)")
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.legend(fontsize=8, ncol=2)
    ax.grid(True, linestyle=":", alpha=0.6)
    fig.tight_layout()
    fig.savefig(filename, dpi=150)
    print(f"Saved {filename}")
    plt.close(fig)


if HAS_MPL:
    os.makedirs("plots", exist_ok=True)

    make_plot("cmp",   "Количество сравнений",           "Сравнения vs. размер списка",           "plots/comparisons.png")
    make_plot("swaps", "Количество перестановок указ.",   "Перестановки указателей vs. размер",    "plots/pointer_swaps.png")
    make_plot("time",  "Время (сек.)",                    "Время выполнения vs. размер списка",    "plots/time.png")

    # ── Отдельные графики для каждой сортировки ──────────────────────────────
    for sort_prefix in ("insertion_sort", "merge_sort"):
        fig, axes = plt.subplots(1, 3, figsize=(14, 4))
        fig.suptitle("Insertion Sort" if sort_prefix == "insertion_sort" else "Merge Sort")
        metrics = [("cmp", "Сравнения"), ("swaps", "Перестановки"), ("time", "Время (с)")]
        for ax, (mk, mlabel) in zip(axes, metrics):
            for key, vals in data.items():
                if not key.startswith(sort_prefix):
                    continue
                color, marker, ls = STYLES.get(key, ("gray", "x", ":"))
                ax.plot(vals["sizes"], vals[mk],
                        color=color, marker=marker, linestyle=ls,
                        label=LABELS.get(key, key).split("/")[1].strip(),
                        linewidth=1.6, markersize=5)
            ax.set_xlabel("n")
            ax.set_ylabel(mlabel)
            ax.legend(fontsize=8)
            ax.grid(True, linestyle=":", alpha=0.6)
        fig.tight_layout()
        fname = f"plots/{sort_prefix}_detail.png"
        fig.savefig(fname, dpi=150)
        print(f"Saved {fname}")
        plt.close(fig)

else:
    # Текстовая таблица как запасной вариант
    header = f"{'Key':<45} {'n':>6} {'cmp':>10} {'swaps':>10} {'time_s':>10}"
    print(header)
    print("-" * len(header))
    for key, vals in data.items():
        for i, n in enumerate(vals["sizes"]):
            print(f"{key:<45} {n:>6} {vals['cmp'][i]:>10} {vals['swaps'][i]:>10} {vals['time'][i]:>10.6f}")
