# SPDX-License-Identifier: MIT OR Apache-2.0
"""Draws the README benchmark chart from a results.md written by run_bench.py.

Usage: python plot_results.py <results.md> <output dir>
Writes benchmark-light.svg and benchmark-dark.svg (read and write times of
the C++ choices, lasrs-cpp highlighted).
"""

import re
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib import font_manager  # noqa: E402

# Reproducible SVG output: fixed element ids, no creation date (see savefig).
plt.rcParams["svg.hashsalt"] = "lasrs-cpp"

FONT = "IBM Plex Sans"
# conda-forge's font-ttf-ibm-plex-sans installs into <env>/fonts, which
# matplotlib does not search on its own.
FONT_FILES = sorted((Path(sys.prefix) / "fonts").glob("IBMPlexSans-*.ttf"))

HIGHLIGHT = "lasrs-cpp"
# The chart compares what C++ users choose between; these stay in the tables
# only: laspy is Python, laz-perf is the codec PDAL is measured with.
TABLES_ONLY = ("laspy", "laz-perf")
THEMES = {
    "light": {"accent": "#2f81f7", "muted": "#b8c0c8", "text": "#1f2328", "secondary": "#59636e"},
    "dark": {"accent": "#2f81f7", "muted": "#4a5563", "text": "#e6edf3", "secondary": "#9198a1"},
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


def use_plex() -> None:
    if not FONT_FILES:
        sys.exit(f"IBM Plex Sans not found in {Path(sys.prefix) / 'fonts'}; run this through pixi")
    for file in FONT_FILES:
        font_manager.fontManager.addfont(str(file))
    plt.rcParams["font.family"] = FONT


def draw(tables: dict, theme: dict, output: Path, background: str | None = None) -> None:
    """Read above write, sharing the seconds axis, sized to stay readable
    when GitHub scales the README image down."""
    heights = [len(rows) for rows in tables.values()]
    figure, axes = plt.subplots(len(tables), 1, figsize=(8, 0.85 * sum(heights) + 1.6), sharex=True,
                                gridspec_kw={"height_ratios": heights})
    if background:
        figure.patch.set_facecolor(background)
    else:
        figure.patch.set_alpha(0)
    limit = max(seconds for rows in tables.values() for _, seconds in rows) * 1.18
    for axis, (name, rows) in zip(axes, tables.items()):
        rows = rows[::-1]
        ours = [label.startswith(HIGHLIGHT) for label, _ in rows]
        colors = [theme["accent"] if mine else theme["muted"] for mine in ours]
        bars = axis.barh(range(len(rows)), [seconds for _, seconds in rows], color=colors, height=0.62)
        axis.set_yticks(range(len(rows)), [label.replace(", ", "\n", 1) for label, _ in rows])
        for bar, (_, value), mine in zip(bars, rows, ours):
            axis.text(value + limit * 0.012, bar.get_y() + bar.get_height() / 2, f"{value:.1f} s",
                      va="center", fontsize=15, color=theme["text"], fontweight="semibold" if mine else "normal")
        axis.set_title(name, loc="left", fontsize=19, fontweight="semibold", color=theme["text"], pad=10)
        axis.set_xlim(0, limit)
        axis.set_facecolor("none")
        axis.tick_params(colors=theme["secondary"], labelsize=13, length=0, pad=8)
        for label, mine in zip(axis.get_yticklabels(), ours):
            label.set_fontsize(14)
            label.set_color(theme["text"] if mine else theme["secondary"])
            label.set_fontweight("semibold" if mine else "normal")
        for side in ("top", "right", "left"):
            axis.spines[side].set_visible(False)
        axis.spines["bottom"].set_color(theme["muted"])
        axis.tick_params(axis="x", labelbottom=True)
    axes[-1].set_xlabel("seconds, lower is better", fontsize=13, color=theme["secondary"])
    figure.tight_layout(h_pad=2)
    with output.open("wb") as file:
        figure.savefig(file, format=output.suffix[1:], transparent=background is None, dpi=110,
                       metadata={"Date": None})
    plt.close(figure)


def main() -> None:
    results, output_dir = Path(sys.argv[1]), Path(sys.argv[2])
    use_plex()
    tables = chart_rows(results.read_text())
    output_dir.mkdir(parents=True, exist_ok=True)
    for mode, theme in THEMES.items():
        draw(tables, theme, output_dir / f"benchmark-{mode}.svg")
        print(output_dir / f"benchmark-{mode}.svg")


if __name__ == "__main__":
    main()
