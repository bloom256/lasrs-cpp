# SPDX-License-Identifier: MIT OR Apache-2.0
"""Benchmarks laspy with the lazrs backend; keeps the written files.

Usage: python bench_laspy.py <input.laz> <output dir> [runs]
Prints Markdown table rows and appends them to <output dir>/results.md.
"""

import os
import re
import sys
import time
from pathlib import Path

import laspy
from laspy import LazBackend

LIBRARY = "laspy + lazrs"


def best_of(runs: int, action) -> float:
    best = float("inf")
    for _ in range(runs):
        start = time.perf_counter()
        action()
        best = min(best, time.perf_counter() - start)
    return best


def file_name(operation: str) -> str:
    return re.sub(r"[^A-Za-z0-9]", "-", f"{LIBRARY}_{operation}") + ".laz"


def main() -> None:
    path = Path(sys.argv[1])
    output_dir = Path(sys.argv[2])
    runs = int(sys.argv[3]) if len(sys.argv) > 3 else 3
    threads = f"{os.cpu_count()} threads"

    las = laspy.read(path, laz_backend=LazBackend.LazrsParallel)
    count = len(las.points)
    parallel_output = output_dir / file_name(f"write, {threads}")
    sequential_output = output_dir / file_name("write, 1 thread")
    benchmarks = [
        (f"read, {threads}", lambda: laspy.read(path, laz_backend=LazBackend.LazrsParallel)),
        ("read, 1 thread", lambda: laspy.read(path, laz_backend=LazBackend.Lazrs)),
        (f"write, {threads}", lambda: las.write(parallel_output, laz_backend=LazBackend.LazrsParallel)),
        ("write, 1 thread", lambda: las.write(sequential_output, laz_backend=LazBackend.Lazrs)),
    ]
    with (output_dir / "results.md").open("a") as results:
        for operation, action in benchmarks:
            seconds = best_of(runs, action)
            line = f"| {LIBRARY} | {operation} | {seconds:.2f} | {count / seconds / 1e6:.1f} |"
            print(line, flush=True)
            results.write(line + "\n")


if __name__ == "__main__":
    main()
