# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
