# SPDX-License-Identifier: MIT OR Apache-2.0
"""Downloads the default benchmark point cloud into test_data/ if missing.

AHN4 tile 25GN2_18 (Amsterdam), 105.6 million points, LAS 1.4 point
format 8, 952 MB. AHN data is public domain (CC0); tiles are served by
GeoTiles, TU Delft.
"""

import hashlib
import sys
import urllib.request
from pathlib import Path

URL = "https://geotiles.citg.tudelft.nl/AHN4_T/25GN2_18.LAZ"
SIZE = 998_367_953
SHA256 = "4ed8ddd6b4ec8e402b1682c29a66ba3998c60ddf22d54e47dfc7e8f1973b64cb"
TARGET = Path(__file__).resolve().parent.parent / "test_data" / "ahn4_25GN2_18.laz"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for block in iter(lambda: file.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def download(target: Path) -> None:
    partial = target.with_suffix(".part")
    with urllib.request.urlopen(URL) as response, partial.open("wb") as file:
        done = 0
        while block := response.read(1 << 20):
            file.write(block)
            done += len(block)
            print(f"\rdownloading {URL}: {done / 1e6:.0f} / {SIZE / 1e6:.0f} MB", end="", flush=True)
    print()
    partial.replace(target)


def main() -> None:
    TARGET.parent.mkdir(exist_ok=True)
    if not TARGET.exists() or TARGET.stat().st_size != SIZE:
        download(TARGET)
    if TARGET.stat().st_size != SIZE or (SHA256 and sha256(TARGET) != SHA256):
        sys.exit(f"{TARGET} does not match the expected download; delete it and retry")
    print(f"benchmark data: {TARGET}")


if __name__ == "__main__":
    main()
