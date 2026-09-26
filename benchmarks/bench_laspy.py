# SPDX-License-Identifier: MIT OR Apache-2.0
"""Benchmarks laspy with the lazrs backend, one case per process.

Usage: python bench_laspy.py <case> <input.laz> <output dir> <runs>
Prints one tab-separated line like lasrs_bench --case: library, operation,
seconds, points, peak memory growth in bytes. Exits with 3 when there is no
such case.
"""

import os
import re
import sys
import time
from pathlib import Path

import laspy
import psutil
from laspy import LazBackend

LIBRARY = "laspy + lazrs (Python)"
CHUNK = 1_000_000


def peak_memory_bytes() -> int:
    info = psutil.Process().memory_info()
    if hasattr(info, "peak_wset"):
        return info.peak_wset
    import resource

    peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    return peak if sys.platform == "darwin" else peak * 1024


def file_name(operation: str) -> str:
    return re.sub(r"[^A-Za-z0-9]", "-", f"{LIBRARY}_{operation}") + ".laz"


def read(path: Path, backend: LazBackend) -> int:
    count = 0
    with laspy.open(path, laz_backend=backend) as reader:
        for chunk in reader.chunk_iterator(CHUNK):
            count += len(chunk)
    return count


def cases(path: Path, output_dir: Path):
    threads = f"{os.cpu_count()} threads"
    yield f"read, {threads}", lambda: None, lambda _: read(path, LazBackend.LazrsParallel)
    yield "read, 1 thread", lambda: None, lambda _: read(path, LazBackend.Lazrs)
    for operation, backend in ((f"write, {threads}", LazBackend.LazrsParallel), ("write, 1 thread", LazBackend.Lazrs)):
        output = output_dir / file_name(operation)
        yield (
            operation,
            lambda: laspy.read(path, laz_backend=LazBackend.LazrsParallel),
            lambda las, output=output, backend=backend: las.write(output, laz_backend=backend) or len(las.points),
        )


def main() -> None:
    index = int(sys.argv[1])
    path, output_dir, runs = Path(sys.argv[2]), Path(sys.argv[3]), int(sys.argv[4])
    all_cases = list(cases(path, output_dir))
    if index >= len(all_cases):
        sys.exit(3)
    operation, prepare, action = all_cases[index]

    data = prepare()
    baseline = peak_memory_bytes()
    best, points = float("inf"), 0
    for _ in range(runs):
        start = time.perf_counter()
        points = action(data)
        best = min(best, time.perf_counter() - start)
    print(f"{LIBRARY}\t{operation}\t{best}\t{points}\t{peak_memory_bytes() - baseline}")


if __name__ == "__main__":
    main()
