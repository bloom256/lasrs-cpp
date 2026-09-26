# SPDX-License-Identifier: MIT OR Apache-2.0
"""Runs all benchmarks, one case per process, and verifies the written files.

Usage: python run_bench.py [input.laz] [runs]
Without an input, the default AHN4 tile is downloaded (see fetch_data.py).
The tables, the verification and all written files go to
test_data/output/bench/<input name>/ (results.md).
"""

import subprocess
import sys
from pathlib import Path

import fetch_data
import laspy

ROOT = Path(__file__).resolve().parent.parent
BENCH = ROOT / "build" / "bench" / "lasrs_bench"
LASPY = Path(__file__).resolve().parent / "bench_laspy.py"
NO_SUCH_CASE = 3
TABLE_HEADER = (
    "| Library | Threads | Seconds | Million points/s | Peak memory (MB) |\n"
    "|---|---|---:|---:|---:|"
)


def run_cases(program: list, arguments: list) -> list:
    """Runs `program <case> arguments` for case 0, 1, ... until there is none."""
    results = []
    for index in range(1000):
        result = subprocess.run([*program, str(index), *arguments], capture_output=True, text=True)
        if result.returncode == NO_SUCH_CASE:
            return results
        if result.returncode != 0:
            sys.exit(f"benchmark case {index} failed:\n{result.stderr}")
        library, operation, seconds, points, memory = result.stdout.strip().split("\t")
        kind, threads = operation.split(", ", 1)
        results.append(
            {
                "library": library,
                "kind": kind,
                "threads": threads,
                "seconds": float(seconds),
                "points": int(points),
                "memory": int(memory),
            }
        )
        print(f"  {library}, {operation}: {float(seconds):.2f} s", flush=True)
    return results


def table(results: list, kind: str) -> str:
    rows = sorted((r for r in results if r["kind"] == kind), key=lambda r: r["seconds"])
    lines = [
        f"| {r['library']} | {r['threads']} | {r['seconds']:.2f} | "
        f"{r['points'] / r['seconds'] / 1e6:.1f} | {r['memory'] / 2**20:,.0f} |"
        for r in rows
    ]
    return "\n".join([f"**{kind.capitalize()}**\n", TABLE_HEADER, *lines])


def main() -> None:
    if len(sys.argv) > 1:
        path = Path(sys.argv[1]).resolve()
    else:
        fetch_data.main()
        path = fetch_data.TARGET
    runs = sys.argv[2] if len(sys.argv) > 2 else "3"
    output_dir = ROOT / "test_data" / "output" / "bench" / path.stem
    output_dir.mkdir(parents=True, exist_ok=True)

    header = laspy.open(path).header
    title = (
        f"{path.name}: {header.point_count:,} points, point format {header.point_format.id}, "
        f"{path.stat().st_size / 2**20:,.0f} MB, best of {runs} runs"
    )
    print(title, flush=True)
    arguments = [str(path), str(output_dir), runs]
    results = run_cases([str(BENCH), "--case"], arguments)
    results += run_cases([sys.executable, str(LASPY)], arguments)

    verification = subprocess.run([BENCH, "--verify", path, output_dir], capture_output=True, text=True)
    notes = (
        "Peak memory is how much the process grew during the timed operation; for writes it "
        "excludes the input points, which every writer gets already in memory."
    )
    report = "\n\n".join(
        [title, table(results, "read"), table(results, "write"), notes, verification.stdout + verification.stderr]
    )
    print("\n" + report, flush=True)
    (output_dir / "results.md").write_text(report)
    print(f"written files and results.md: {output_dir}")
    sys.exit(verification.returncode)


if __name__ == "__main__":
    main()
