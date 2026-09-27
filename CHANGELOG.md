# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

More of the las-rs 0.11 API, with the las-rs names:

- `Version::supports_point_format`, `Version::supports<F>` and
  `verify_support_for<F>` with the `las::feature` types (`FileSourceId`,
  `GpsStandardTime`, `Waveforms`, `SyntheticReturnNumbers`, `LargeFiles`,
  `Evlrs`).
- `Header(std::istream&)` (`Header::new`) and `Header::write_to`.
- `copc::CopcHierarchyVlr` with `RECORD_ID` and `iter_entries`, and
  `Header::copc_hierarchy_evlr`.
- `copc::USER_ID`, `copc::DESCRIPTION`, `copc::CopcInfoVlr::RECORD_ID`,
  `Vlr::is_copc_info` and `Vlr::is_copc_hierarchy`.
- `laz::is_laszip_vlr`.
- `point::new_classification`, the checked `Classification::new`.
- `operator<<` for `Version`, `point::Format` and `Transform`, printing
  what their `Display` impls print.

## [0.1.1] - 2026-09-27

Fixes from a code review. Upgrading is recommended: several of these
crashed the process instead of throwing `las::Error`.

### Fixed

- Corrupt files no longer abort the process: EVLR sizes and the LAZ chunk
  count are checked against the real data size before las-rs allocates
  from them, and points are read in batches, so a bogus point count fails
  cleanly.
- `Header(Version)` with a version las-rs cannot build and
  `PointData::resize_for` with a size that overflows throw instead of
  aborting.
- Reading after `Reader::seek` past the last point returns no points
  instead of failing.
- `CopcReader::read_entry` accepts only entries from the file's hierarchy.
- Readers and writers on `std::istream` / `std::ostream` work when the
  data does not start at position 0 and when the stream has `exceptions()`
  enabled; unusable streams are rejected with a clear error.
- Views returned by `Writer::header()` (e.g. `system_identifier()`) stay
  valid while the writer lives.
- `find_package(lasrs)` can be called more than once.
- lasrs-cpp works as a CMake subproject (`add_subdirectory` /
  `FetchContent`) without adding its files to the parent's install
  (`LASRS_INSTALL`); `lasrs::lasrs_ffi` exists there too.
- The static-CRT Windows bundle links with `/MTd` as well as `/MT`, and
  the README download snippet detects
  `MultiThreaded$<$<CONFIG:Debug>:Debug>` as a static CRT.
- Headers compile cleanly with `-Wshadow -Wconversion -Wsign-conversion`;
  VLR user id checks no longer depend on the C locale.

### Changed

- The Rust crate is built with the committed `Cargo.lock` (`--locked`).
- The README states the API scope precisely; the parts of the las-rs API
  still missing are listed in docs/ROADMAP.md.
- SECURITY.md describes the remaining known gap with untrusted COPC files.

## [0.1.0] - 2026-09-26

First release: the las-rs 0.11 API in C++.

### Added

- `las::Reader`: read LAS/LAZ from a path or any `std::istream`, in batches
  (`read_points`, `fill_points`, `read_all`), with `seek`; LAZ is
  decompressed in parallel on all cores.
- `las::Writer`: write LAS/LAZ to a path or any `std::ostream`, point by
  point or in batches; LAZ is compressed in parallel.
- `las::PointData` and `las::PointDataBuilder`: points as structs, as
  columns (`x()`, `intensity()`, `gps_time()`, ...) or as raw records.
- `las::Header` and `las::Builder`, including VLRs / EVLRs, WKT CRS and
  GeoTIFF CRS keys (`las::crs`).
- `las::CopcReader`: COPC hierarchy access and queries by level of detail
  and bounds.
- Value types mirroring las-rs: `Point`, `point::Format`, `Classification`,
  `Transform`, `Bounds`, `Vlr`, `copc::VoxelKey`, ...
- A C API (`lasrs.h`) under the C++ wrapper.
- Prebuilt bundles (static library, headers, CMake package) for Windows x64
  (`/MD` and `/MT`), Linux x64 and arm64 (glibc 2.28+), and macOS arm64 and
  x64.
- Benchmarks against LASzip, laz-perf, PDAL and laspy (`benchmarks/`).
