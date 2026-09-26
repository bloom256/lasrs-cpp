# lasrs-cpp

[![CI](https://github.com/bloom256/lasrs-cpp/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/bloom256/lasrs-cpp/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/bloom256/lasrs-cpp?include_prereleases&sort=semver)](https://github.com/bloom256/lasrs-cpp/releases)
[![License: MIT OR Apache-2.0](https://img.shields.io/badge/license-MIT%20OR%20Apache--2.0-blue.svg)](#license)

Fast LAS / LAZ / COPC reading and writing for C++, powered by the Rust
crate [las-rs](https://github.com/gadomski/las-rs) and its parallel LAZ
codec [laz-rs](https://github.com/tmontaigu/laz-rs).

> Status: early development. The API mirrors las-rs 0.11 but is not
> stable yet.

## Why

las-rs decompresses LAZ chunks in parallel on all cores. lasrs-cpp brings
that speed to C++ without re-implementing the codec: the Rust crates sit
behind a small C ABI and a modern C++20 API.

On a public 952 MB LAZ file with 105.6 million points, lasrs-cpp reads
3.6x faster than PDAL and 5.7x faster than LASzip, and writes 4.3x faster
than LASzip and 7.9x faster than PDAL (see [Performance](#performance)).

## Features

- The las-rs API in C++, with the same names: if you know las-rs, you
  know lasrs-cpp
- LAS 1.0 - 1.4, point formats 0 - 10, extra bytes, waveform fields
- LAZ read and write, parallel over chunks
- COPC read: hierarchy access, level-of-detail and bounds queries
- Header, VLR / EVLR, WKT CRS
- Bulk access: read points in batches, as `Point` structs, as columns
  (`x()`, `intensity()`, ...) or as raw record bytes
- Read from paths or any `std::istream`, write to paths or any
  `std::ostream`
- Header-only C++20 wrapper with RAII and exceptions, plus a plain C API
- Static linking: no Rust runtime and no extra DLLs in your application

## Examples

Read a file in batches:

```cpp
#include <lasrs/lasrs.hpp>

#include <iostream>

int main()
{
    auto reader = las::Reader::from_path("cloud.laz");
    std::cout << reader.header().number_of_points() << " points\n";

    auto points = las::PointDataBuilder().for_header(reader.header()).build();
    while (reader.fill_points(1'000'000, points) != 0)
    {
        for (double z : points.z())
        {
            // ...
        }
    }
}
```

Write a LAZ file:

```cpp
las::Builder builder(las::Version(1, 4));
builder.point_format = las::point::Format(6);
auto writer = las::Writer::from_path("out.laz", builder.into_header());

las::Point point;
point.x = 1.0;
point.gps_time = 42.0;
writer.write_point(point);
writer.close();
```

Query a COPC file:

```cpp
auto reader = las::CopcReader::from_path("cloud.copc.laz");
const las::Bounds area{{637000, 851000, 0}, {638000, 852000, 1000}};
auto points = reader.query(las::LodSelection::Resolution(1.0), las::BoundsSelection::Within(area));
```

## Performance

AHN4 tile 25GN2_18 (Amsterdam, public domain): 105.6 million points, LAS 1.4
point format 8, 952 MB. Intel Core i7-10750H (6 cores, 12 threads), 32 GB,
Windows 11; best of 3 runs, each case in its own process, file in the OS
cache.

**Read**

| Library | Threads | Seconds | Million points/s | Peak memory (MB) |
|---|---|---:|---:|---:|
| **lasrs-cpp** | **12** | **11.5** | **9.2** | 99 |
| laspy 2.7 + lazrs | 12 | 12.6 | 8.4 | 139 |
| PDAL 2.10 | 7 (default) | 41.2 | 2.6 | 84 |
| PDAL 2.10 | 12 | 43.1 | 2.5 | 126 |
| laz-perf 3.4 | 1 | 63.6 | 1.7 | 16 |
| LASzip 3.4 | 1 | 65.6 | 1.6 | 1 |
| lasrs-cpp | 1 | 66.0 | 1.6 | 47 |
| PDAL 2.10 | 1 | 67.1 | 1.6 | 33 |
| laspy 2.7 + lazrs | 1 | 67.6 | 1.6 | 89 |

**Write**

| Library | Threads | Seconds | Million points/s | Peak memory (MB) |
|---|---|---:|---:|---:|
| **lasrs-cpp** | **12** | **9.8** | **10.8** | 89 |
| laspy 2.7 + lazrs | 12 | 10.9 | 9.7 | 84 |
| lasrs-cpp | 1 | 39.3 | 2.7 | 0 |
| laspy 2.7 + lazrs | 1 | 41.7 | 2.5 | 0 |
| LASzip 3.4 | 1 | 42.2 | 2.5 | 136 |
| laz-perf 3.4 | 1 | 46.8 | 2.3 | 0 |
| PDAL 2.10 | 1 | 77.9 | 1.4 | 10 |

- Single-threaded, all LAZ codecs are about equally fast; lasrs-cpp wins by
  using all cores.
- Parallel decoding costs memory: about 100 MB instead of 47 MB for
  lasrs-cpp, because several LAZ chunks are decompressed at once. Reads
  stream in 1M-point batches, so memory does not grow with the file.
- Peak memory is how much the process grew during the timed operation;
  write benchmarks get their input points already in memory, which is not
  counted.
- Every written file was checked to hold exactly the input points. PDAL's
  writer drops extra bytes by default; its standard fields are identical.

Reproduce with [benchmarks/](benchmarks/README.md): `pixi run bench`
downloads this file and writes the tables, the verification and all written
files to `test_data/output/bench/`.

## Mapping from las-rs

Names and semantics follow las-rs. Where Rust and C++ differ:

| las-rs | lasrs-cpp |
|---|---|
| `las::Reader::from_path(p)` | `las::Reader::from_path(p)` |
| `X::new(...)` | constructor `X(...)` |
| `Result<T>` | returns `T`, throws `las::Error` |
| `Option<T>` | `std::optional<T>` |
| `&str`, `&[u8]` | `std::string_view`, `std::span<const uint8_t>` |
| iterator (`pd.x()`) | `std::vector` |
| `impl Read + Seek` / `impl Write + Seek` | `std::istream&` / `std::ostream&` |
| method on an enum (`t.is_standard()`) | free function (`is_standard(t)`) |
| `NaiveDate`, `Uuid` | `std::chrono::year_month_day`, `std::array<uint8_t, 16>` |

las-rs cannot write COPC, so neither can lasrs-cpp.

## Getting it

### Prebuilt binaries (no Rust needed)

Planned: each release will ship static libraries and headers for common
platforms, consumable from CMake.

### From source (needs Rust)

```cmake
include(FetchContent)
FetchContent_Declare(lasrs
  GIT_REPOSITORY https://github.com/bloom256/lasrs-cpp.git
  GIT_TAG main)
FetchContent_MakeAvailable(lasrs)
target_link_libraries(my_app PRIVATE lasrs::lasrs)
```

See [docs/BUILDING.md](docs/BUILDING.md) for the toolchain.

## Project practices

- Every push and pull request is built and tested by GitHub Actions on
  Windows, Linux and macOS; the CI badge above shows the current status.
- Formatting (cargo fmt, clang-format), linting (clippy), a check that
  the committed C header is up to date, license checks (cargo deny) and
  sanitizers run in CI.
- Planned: automated releases from version tags, Dependabot, CodeQL and
  a protected `main` branch.
- Semantic versioning and a human-readable [CHANGELOG](CHANGELOG.md).

## Documentation

- [docs/BUILDING.md](docs/BUILDING.md) - toolchain and build instructions
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) - how the pieces fit together
- [docs/ROADMAP.md](docs/ROADMAP.md) - milestones and open questions
- [CONTRIBUTING.md](CONTRIBUTING.md) - how to contribute
- [CHANGELOG.md](CHANGELOG.md) - release notes

## License

Licensed under either of

- Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE))
- MIT license ([LICENSE-MIT](LICENSE-MIT))

at your option.

The static library also contains code from third-party Rust crates
(las-rs is MIT, laz-rs is Apache-2.0); their licenses must be preserved
when distributing binaries.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally
submitted for inclusion in the work by you, as defined in the Apache-2.0
license, shall be dual licensed as above, without any additional terms or
conditions.
