import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("benchmark_results.csv")

data   = [df["naive"], df["kmp"], df["bm"]]
labels = ["Наивный", "КМП", "Бойер–Мур"]
colors = ["#AED6F1", "#A9DFBF", "#F9E79F"]

fig, axes = plt.subplots(1, 3, figsize=(12, 6))
fig.suptitle("Диаграмма размаха времени выполнения алгоритмов поиска подстроки", fontsize=13, y=1.02)
for ax, series, label, color in zip(axes, data, labels, colors):
    ax.boxplot(series, patch_artist=True, widths=0.5,
               boxprops=dict(facecolor=color, color="#2C3E50"),
               medianprops=dict(color="#E74C3C", linewidth=2),
               whiskerprops=dict(color="#2C3E50"),
               capprops=dict(color="#2C3E50"),
               flierprops=dict(marker="o", color="#E74C3C", markersize=5))

    ax.set_title(label, fontsize=12)
    ax.set_ylabel("Время (мкс)")
    ax.yaxis.grid(True, linestyle="--", alpha=0.7)
    ax.set_axisbelow(True)
    ax.set_xticks([])

plt.tight_layout()
plt.savefig("boxplot.png", dpi=150)
print("Сохранено: boxplot.png")