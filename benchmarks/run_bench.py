# SPDX-License-Identifier: MIT OR Apache-2.0
"""Runs all benchmarks and verifies the written files.

Usage: python run_bench.py [input.laz] [runs]
Without an input, the default AHN4 tile is downloaded (see fetch_data.py).
Results and all written files go to test_data/output/bench/<input name>/.
"""

import subprocess
import sys
from pathlib import Path

import fetch_data

ROOT = Path(__file__).resolve().parent.parent
BENCH = ROOT / "build" / "bench" / "lasrs_bench"


def main() -> None:
    if len(sys.argv) > 1:
        path = Path(sys.argv[1]).resolve()
    else:
        fetch_data.main()
        path = fetch_data.TARGET
    runs = sys.argv[2] if len(sys.argv) > 2 else "3"
    output_dir = ROOT / "test_data" / "output" / "bench" / path.stem
    output_dir.mkdir(parents=True, exist_ok=True)

    subprocess.run([BENCH, path, output_dir, runs], check=True)
    subprocess.run([sys.executable, Path(__file__).parent / "bench_laspy.py", path, output_dir, runs], check=True)
    verified = subprocess.run([BENCH, "--verify", path, output_dir])
    print(f"\nwritten files and results.md: {output_dir}")
    sys.exit(verified.returncode)


if __name__ == "__main__":
    main()
