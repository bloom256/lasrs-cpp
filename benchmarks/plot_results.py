# SPDX-License-Identifier: MIT OR Apache-2.0
"""Draws the README benchmark chart from a results.md written by run_bench.py.

Usage: python plot_results.py <results.md> <output dir>
Writes benchmark-light.svg and benchmark-dark.svg: read and write time per
library, each library in its fastest configuration, lasrs-cpp highlighted.
"""

import re
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

HIGHLIGHT = "lasrs-cpp"
THEMES = {
    "light": {"accent": "#2a78d6", "muted": "#8a8984", "text": "#0b0b0b", "secondary": "#52514e"},
    "dark": {"accent": "#3987e5", "muted": "#6e6d68", "text": "#ffffff", "secondary": "#c3c2b7"},
}
ROW = re.compile(r"^\| (?P<library>[^|]+) \| (?P<threads>[^|]+) \| (?P<seconds>[\d.]+) \|")


def fastest_per_library(results: str) -> dict:
    """Maps "Read"/"Write" to [(label, seconds)] sorted fastest first."""
    tables, section = {}, None
    for line in results.splitlines():
        heading = re.match(r"^\*\*(Read|Write)\*\*$", line)
        if heading:
            section = heading.group(1)
            tables[section] = {}
            continue
        row = ROW.match(line)
        if section and row:
            library, threads, seconds = row["library"].strip(), row["threads"].strip(), float(row["seconds"])
            if library not in tables[section] or seconds < tables[section][library][1]:
                tables[section][library] = (f"{library}, {threads}", seconds)
    return {name: sorted(rows.values(), key=lambda row: row[1]) for name, rows in tables.items()}


def draw(tables: dict, theme: dict, output: Path, background: str | None = None) -> None:
    figure, axes = plt.subplots(1, len(tables), figsize=(10, 3.2), sharex=True)
    if background:
        figure.patch.set_facecolor(background)
    else:
        figure.patch.set_alpha(0)
    limit = max(seconds for rows in tables.values() for _, seconds in rows) * 1.18
    for axis, (name, rows) in zip(axes, tables.items()):
        labels = [label for label, _ in rows][::-1]
        seconds = [value for _, value in rows][::-1]
        colors = [theme["accent"] if label.startswith(HIGHLIGHT) else theme["muted"] for label in labels]
        bars = axis.barh(labels, seconds, color=colors, height=0.62)
        for bar, value in zip(bars, seconds):
            axis.text(value + limit * 0.015, bar.get_y() + bar.get_height() / 2, f"{value:.1f} s",
                      va="center", fontsize=9, color=theme["text"])
        axis.set_title(name, loc="left", fontsize=12, fontweight="bold", color=theme["text"])
        axis.set_xlim(0, limit)
        axis.set_facecolor("none")
        axis.tick_params(colors=theme["secondary"], labelsize=9, length=0)
        for label in axis.get_yticklabels():
            label.set_color(theme["text"] if label.get_text().startswith(HIGHLIGHT) else theme["secondary"])
        for side in ("top", "right", "left"):
            axis.spines[side].set_visible(False)
        axis.spines["bottom"].set_color(theme["secondary"])
        axis.set_xlabel("seconds, lower is better", fontsize=9, color=theme["secondary"])
    figure.tight_layout(w_pad=3)
    figure.savefig(output, transparent=background is None, dpi=110)
    plt.close(figure)


def main() -> None:
    results, output_dir = Path(sys.argv[1]), Path(sys.argv[2])
    tables = fastest_per_library(results.read_text())
    output_dir.mkdir(parents=True, exist_ok=True)
    for mode, theme in THEMES.items():
        draw(tables, theme, output_dir / f"benchmark-{mode}.svg")
        print(output_dir / f"benchmark-{mode}.svg")


if __name__ == "__main__":
    main()
