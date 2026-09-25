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
- [ ] GeoTIFF CRS: `get_geotiff_crs`

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

- [ ] build + test matrix: Windows (MSVC), Linux (GCC, Clang), macOS
      (Apple Clang, arm64); Debug and Release
- [ ] `ctest` for the C++ tests
- [ ] `cargo fmt --check`, `cargo clippy -D warnings`, clang-format check
- [ ] cbindgen check: committed `lasrs.h` must match the generated one
- [ ] `cargo deny check` (licenses, advisories, bans, sources)
- [ ] sanitizers job (ASan / UBSan) on Linux
- [ ] caching of cargo downloads
- [x] status badge in README

## M6 - Compiled library bundle

No package manager: users take the compiled static library and headers.

- [ ] CMake install rules: `include/lasrs/*`, the static library, and a
      CMake package config (`find_package(lasrs)`) for convenience
- [ ] `THIRD-PARTY-NOTICES` (`cargo about`) and LICENSE files in the bundle
- [ ] docs: how to link the bundle without CMake (system libraries needed
      per platform)
- [ ] later, when publishing is wanted: CI builds the bundle per platform
      (x64-windows /MD and /MT, x64-linux, arm64-macos) and attaches it to
      GitHub releases

## M7 - Repository hygiene

- [ ] branch protection on `main`: PRs required, CI must pass
- [ ] Dependabot for GitHub Actions and Cargo dependencies
- [ ] CodeQL code scanning
- [ ] issue templates, pull request template
- [ ] SECURITY.md, CODE_OF_CONDUCT.md

## Out of scope

- COPC writing: las-rs has no COPC writer.
- LAZ files without a chunk table: las-rs rejects them in parallel mode
  and laz-rs 0.13 fails on them sequentially (upstream bug).
