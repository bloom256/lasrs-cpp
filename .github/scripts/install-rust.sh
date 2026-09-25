#!/usr/bin/env bash
# Installs the toolchain pinned in rust-toolchain.toml.
set -euo pipefail
channel=$(sed -n 's/^channel = "\(.*\)"/\1/p' rust-toolchain.toml)
rustup toolchain install "$channel" --profile minimal --component rustfmt,clippy
rustup show active-toolchain
