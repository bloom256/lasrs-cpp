# Roadmap

Scope: everything las-rs 0.11 offers, mirrored in C++; nothing more.

## M0 - Scaffold

- [x] git repo, licenses, docs
- [x] `rust-toolchain.toml`, shim crate
- [x] CMake + Corrosion, `CMakePresets.json`
- [x] cbindgen config, generated `lasrs.h`
- [x] `lasrs_abi_version()`, panic guard, `lasrs_last_error()`
- [x] `.gitattributes` (LF line endings), `.clang-format`

## M1 - Read

- [x] `Reader::from_path`, from `std::istream`, `with_options`
- [x] `read_points`, `read_all`, `fill_points`, `seek`
- [x] parallel LAZ decompression
- [x] example `lasrs_info`

## M2 - Data model

- [x] `Header`: all getters, VLRs / EVLRs, `add_point`, `add_point_data`
- [x] `Builder`, including hidden fields carried over from a `Header`
- [x] `PointData`: points, columns, raw bytes, `resize_for`
- [x] `PointDataBuilder`
- [x] value types: `Point`, `point::Format`, `Classification`,
      `Transform`, `Bounds`, `Vector`, `Color`, `Version`, `Vlr`,
      `raw::point::Waveform`
- [x] WKT CRS: `set_wkt_crs`, `get_wkt_crs_bytes`, `remove_crs_vlrs`
- [x] GeoTIFF CRS: `get_geotiff_crs`, `crs::GeoTiffCrs`

## M3 - Write

- [x] `Writer::from_path`, to `std::ostream`, `with_options`
- [x] `write_point`, `write_points`, `close`, `header`
- [x] parallel LAZ compression
- [x] round-trip tests for LAS 1.0 - 1.4, LAZ and extra bytes

## M4 - COPC

- [x] `CopcReader` from path and `std::istream`
- [x] hierarchy entries, `read_entry`, `query` with LOD and bounds
- [x] `copc::VoxelKey`, `copc::Entry`, `copc::CopcInfoVlr`

## M5 - CI (GitHub Actions)

### ci.yml - on every push and pull request

- [x] build + test matrix: Windows (MSVC), Linux (GCC, Clang), macOS
      (Apple Clang, arm64); Debug and Release
- [x] `ctest` for the C++ tests
- [x] `cargo fmt --check`, `cargo clippy -D warnings`, clang-format check
- [x] cbindgen check: committed `lasrs.h` must match the generated one
- [x] `cargo deny check` (licenses, advisories, bans, sources)
- [x] sanitizers job (ASan / UBSan) on Linux
- [x] caching of cargo downloads
- [x] status badge in README

## M6 - Compiled library bundle

No package manager: users take the compiled static library and headers.

- [x] CMake install rules: `include/lasrs/*`, the static library, and a
      CMake package config (`find_package(lasrs)`) for convenience
- [x] `THIRD-PARTY-NOTICES` (`cargo about`) and LICENSE files in the bundle
- [x] CI builds the bundle for windows-x64, linux-x64, linux-arm64,
      macos-arm64 and macos-x64, tests it with separate C and C++ programs
      and keeps it as a workflow artifact for 30 days (not published)
- [x] CI tests the Linux bundles on Ubuntu 20.04/22.04/24.04, Debian 12,
      Fedora, Rocky Linux 8/9 and Arch Linux (x64 and arm64)
- [x] docs: how to link the bundle without CMake (system libraries needed
      per platform)
- [x] release workflow on version tags: every platform's bundle (plus a
      /MT static-CRT Windows variant) attached to the GitHub release, with
      checksums, provenance and a README table mapping OS/arch to the
      download; see docs/RELEASING.md
- [x] first release: v0.1.0 (2026-09-26)

## M7 - Repository hygiene

- [x] branch protection on `main` (ruleset "Protect main"): no force-push or deletion; PRs with passing CI required, repo admins may push directly
- [x] Dependabot for GitHub Actions and Cargo dependencies
- [x] CodeQL code scanning (C/C++, Rust, workflows)
- [x] issue templates, pull request template
- [x] SECURITY.md, CODE_OF_CONDUCT.md

## Out of scope

- COPC writing: las-rs has no COPC writer.
- LAZ files without a chunk table: las-rs rejects them in parallel mode
  and laz-rs 0.13 fails on them sequentially (upstream bug).
