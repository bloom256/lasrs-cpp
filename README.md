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

For example, a 502 MB LAZ file with 37.6 million points is read in about
3.4 seconds on a 12-thread desktop.

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
