# SPDX-License-Identifier: MIT OR Apache-2.0
"""Prints Markdown table rows for laspy with the lazrs backend.

Usage: python bench_laspy.py <file.laz> [runs]
"""

import os
import sys
import time
from pathlib import Path

import laspy
from laspy import LazBackend


def best_of(runs: int, action) -> float:
    best = float("inf")
    for _ in range(runs):
        start = time.perf_counter()
        action()
        best = min(best, time.perf_counter() - start)
    return best


def main() -> None:
    path = Path(sys.argv[1])
    runs = int(sys.argv[2]) if len(sys.argv) > 2 else 3
    output = path.parent / "output" / "laspy_bench.laz"
    output.parent.mkdir(exist_ok=True)
    threads = f"{os.cpu_count()} threads"

    las = laspy.read(path, laz_backend=LazBackend.LazrsParallel)
    count = len(las.points)
    benchmarks = [
        (f"read, {threads}", lambda: laspy.read(path, laz_backend=LazBackend.LazrsParallel)),
        ("read, 1 thread", lambda: laspy.read(path, laz_backend=LazBackend.Lazrs)),
        (f"write, {threads}", lambda: las.write(output, laz_backend=LazBackend.LazrsParallel)),
        ("write, 1 thread", lambda: las.write(output, laz_backend=LazBackend.Lazrs)),
    ]
    for operation, action in benchmarks:
        seconds = best_of(runs, action)
        print(f"| laspy + lazrs | {operation} | {seconds:.2f} | {count / seconds / 1e6:.1f} |")
    output.unlink()


if __name__ == "__main__":
    main()
