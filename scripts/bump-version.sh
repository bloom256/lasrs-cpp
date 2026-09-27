#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
# Sets the project version everywhere it is spelled out and dates the
# changelog: scripts/bump-version.sh X.Y.Z
set -euo pipefail

version=${1:?usage: $0 X.Y.Z}
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "not a version: $version" >&2; exit 1; }
today=$(date +%Y-%m-%d)
cd "$(dirname "$0")/.."

grep -q "^## \[Unreleased\]$" CHANGELOG.md || { echo "CHANGELOG.md has no [Unreleased] section" >&2; exit 1; }

sed -i "s/^  VERSION [0-9.]*$/  VERSION $version/" CMakeLists.txt
sed -i "0,/^version = \".*\"$/s//version = \"$version\"/" rust/Cargo.toml
cargo update --manifest-path rust/Cargo.toml -p lasrs_ffi --offline
sed -i "s/^version: .*$/version: $version/; s/^date-released: .*$/date-released: $today/" CITATION.cff
sed -i "s/v[0-9]\+\.[0-9]\+\.[0-9]\+/v$version/g" README.md
sed -i "s/^## \[Unreleased\]$/## [Unreleased]\n\n## [$version] - $today/" CHANGELOG.md

git --no-pager diff --stat
