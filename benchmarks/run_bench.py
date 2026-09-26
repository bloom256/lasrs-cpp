# SPDX-License-Identifier: MIT OR Apache-2.0
"""Runs all benchmarks, one case per process, and verifies the written files.

Usage: python run_bench.py [input.laz] [runs]
Without an input, the default AHN4 tile is downloaded (see fetch_data.py).
The table, the verification and all written files go to
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


def run_cases(program: list, arguments: list) -> list:
    """Runs `program <case> arguments` for case 0, 1, ... until there is none."""
    rows = []
    for index in range(1000):
        result = subprocess.run([*program, str(index), *arguments], capture_output=True, text=True)
        if result.returncode == NO_SUCH_CASE:
            return rows
        if result.returncode != 0:
            sys.exit(f"benchmark case {index} failed:\n{result.stderr}")
        library, operation, seconds, points, memory = result.stdout.strip().split("\t")
        row = (
            f"| {library} | {operation} | {float(seconds):.2f} | "
            f"{int(points) / float(seconds) / 1e6:.1f} | {int(memory) / 2**20:,.0f} |"
        )
        print(row, flush=True)
        rows.append(row)
    return rows


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
        f"{path.stat().st_size / 2**20:,.0f} MB, best of {runs} runs\n\n"
        "| Library | Operation | Seconds | Million points/s | Peak memory (MB) |\n"
        "|---|---|---:|---:|---:|"
    )
    print(title, flush=True)
    arguments = [str(path), str(output_dir), runs]
    rows = run_cases([str(BENCH), "--case"], arguments)
    rows += run_cases([sys.executable, str(LASPY)], arguments)

    print(flush=True)
    verification = subprocess.run([BENCH, "--verify", path, output_dir], capture_output=True, text=True)
    print(verification.stdout, verification.stderr, sep="", flush=True)

    notes = (
        "\nPeak memory is how much the process grew during the timed operation; for writes it "
        "excludes the input points, which every writer gets already in memory.\n"
    )
    (output_dir / "results.md").write_text("\n".join([title, *rows, notes, verification.stdout]))
    print(f"written files and results.md: {output_dir}")
    sys.exit(verification.returncode)


if __name__ == "__main__":
    main()
