# SPDX-License-Identifier: MIT OR Apache-2.0
"""Draws the README benchmark chart from a results.md written by run_bench.py.

Usage: python plot_results.py <results.md> <output dir>
Writes benchmark-light.svg and benchmark-dark.svg (read and write times of
the C++ choices, lasrs-cpp highlighted)
and social-preview.png, the 1280x640 card for GitHub's social preview.
"""

import re
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

# Reproducible SVG output: fixed element ids, no creation date (see savefig).
plt.rcParams["svg.hashsalt"] = "lasrs-cpp"

HIGHLIGHT = "lasrs-cpp"
# The chart compares what C++ users choose between; these stay in the tables
# only: laspy is Python, laz-perf is the codec PDAL is measured with.
TABLES_ONLY = ("laspy", "laz-perf")
THEMES = {
    "light": {"accent": "#2a78d6", "muted": "#8a8984", "text": "#0b0b0b", "secondary": "#52514e"},
    "dark": {"accent": "#3987e5", "muted": "#6e6d68", "text": "#ffffff", "secondary": "#c3c2b7"},
}
ROW = re.compile(r"^\| (?P<library>[^|]+) \| (?P<threads>[^|]+) \| (?P<seconds>[\d.]+) \|")


def chart_rows(results: str) -> dict:
    """Maps "Read"/"Write" to [(label, seconds)] sorted fastest first.

    A library appears with all its multi-threaded configurations, or with
    its single-threaded one if that is all it has.
    """
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
            if not library.startswith(TABLES_ONLY):
                tables[section].setdefault(library, []).append((f"{library}, {threads}", seconds, threads))
    charts = {}
    for name, libraries in tables.items():
        rows = []
        for configurations in libraries.values():
            parallel = [c for c in configurations if c[2] != "1 thread"]
            rows += [(label, seconds) for label, seconds, _ in (parallel or configurations)]
        charts[name] = sorted(rows, key=lambda row: row[1])
    return charts


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
    with output.open("wb") as file:
        figure.savefig(file, format=output.suffix[1:], transparent=background is None, dpi=110,
                       metadata={"Date": None})
    plt.close(figure)


def draw_social(tables: dict, theme: dict, output: Path) -> None:
    figure = plt.figure(figsize=(12.8, 6.4), dpi=100)
    figure.patch.set_facecolor("#fcfcfb")
    figure.text(0.06, 0.84, "lasrs-cpp", fontsize=54, fontweight="bold", color=theme["text"])
    figure.text(0.06, 0.74, "Fast LAS / LAZ / COPC for C++: the las-rs API, parallel LAZ",
                fontsize=22, color=theme["secondary"])
    axis = figure.add_axes((0.30, 0.14, 0.60, 0.46))
    rows = tables["Read"]
    labels = [label for label, _ in rows][::-1]
    seconds = [value for _, value in rows][::-1]
    colors = [theme["accent"] if label.startswith(HIGHLIGHT) else theme["muted"] for label in labels]
    bars = axis.barh(labels, seconds, color=colors, height=0.62)
    for bar, value in zip(bars, seconds):
        axis.text(value + max(seconds) * 0.015, bar.get_y() + bar.get_height() / 2, f"{value:.1f} s",
                  va="center", fontsize=18, color=theme["text"])
    axis.set_xlim(0, max(seconds) * 1.2)
    axis.set_title("Reading 105.6 million points (LAZ, 952 MB)", loc="left", fontsize=18, color=theme["text"])
    axis.set_facecolor("none")
    axis.tick_params(labelsize=16, length=0, colors=theme["secondary"])
    axis.set_xticks([])
    for label in axis.get_yticklabels():
        label.set_color(theme["text"] if label.get_text().startswith(HIGHLIGHT) else theme["secondary"])
    for side in axis.spines.values():
        side.set_visible(False)
    figure.savefig(output, dpi=100)
    plt.close(figure)


def main() -> None:
    results, output_dir = Path(sys.argv[1]), Path(sys.argv[2])
    tables = chart_rows(results.read_text())
    output_dir.mkdir(parents=True, exist_ok=True)
    for mode, theme in THEMES.items():
        draw(tables, theme, output_dir / f"benchmark-{mode}.svg")
        print(output_dir / f"benchmark-{mode}.svg")
    draw_social(tables, THEMES["light"], output_dir / "social-preview.png")
    print(output_dir / "social-preview.png")


if __name__ == "__main__":
    main()
