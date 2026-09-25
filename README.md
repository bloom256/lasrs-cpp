# lasrs-cpp

[![CI](https://github.com/bloom256/lasrs-cpp/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/bloom256/lasrs-cpp/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/bloom256/lasrs-cpp?include_prereleases&sort=semver)](https://github.com/bloom256/lasrs-cpp/releases)
[![License: MIT OR Apache-2.0](https://img.shields.io/badge/license-MIT%20OR%20Apache--2.0-blue.svg)](#license)

Fast LAS / LAZ / COPC reading and writing for C++, powered by the Rust
crates [las-rs](https://github.com/gadomski/las-rs),
[laz-rs](https://github.com/laz-rs/laz-rs) and
[copc-rs](https://github.com/pka/copc-rs).

> Status: early development. The API is not stable yet.

## Why

The Rust LAS ecosystem decompresses LAZ chunks in parallel. In practice a
file that takes about a minute to open in a typical C++ tool loads in
seconds through the same Rust code (for example laspy with the `lazrs`
backend). lasrs-cpp brings that speed to C++ without re-implementing the
codec: it wraps the Rust crates behind a small C ABI and a modern C++ API.

## Features

Planned for v1.0 (see [docs/ROADMAP.md](docs/ROADMAP.md)):

- LAS 1.0 - 1.4, point formats 0 - 10
- LAZ read and write, parallel over chunks (multi-core)
- COPC read: spatial bounds and level-of-detail queries
- Header, VLR / EVLR, CRS (WKT / GeoTIFF keys), extra bytes
- Batch, zero-copy API: points are decoded straight into your buffers
- Header-only C++17 wrapper with RAII and exceptions, plus a plain C API
- Static linking: no Rust runtime, no extra DLLs in your application

## Example (planned API)

```cpp
#include <lasrs/lasrs.hpp>

int main() {
    lasrs::Reader reader("cloud.laz");
    const auto& h = reader.header();

    std::vector<std::byte> buf(h.point_record_length() * 1'000'000);
    while (auto n = reader.read_raw(buf)) {
        // n points decoded in parallel into buf
    }
}
```

## Getting it

### Prebuilt binaries (no Rust needed)

Each release ships static libraries and headers for common platforms.
With CMake:

```cmake
include(FetchContent)
FetchContent_Declare(lasrs
  URL https://github.com/bloom256/lasrs-cpp/releases/download/vX.Y.Z/lasrs-cpp-x64-windows-md.zip)
FetchContent_MakeAvailable(lasrs)
target_link_libraries(my_app PRIVATE lasrs::lasrs)
```

### From source (needs Rust)

See [docs/BUILDING.md](docs/BUILDING.md).

## Project practices

The project follows common GitHub best practices:

- Every push and pull request is built and tested by GitHub Actions on
  Windows, Linux and macOS; the CI badge above shows the current status.
- Formatting, linting (clippy, clang-format), license checks
  (cargo deny) and sanitizers run in CI.
- Releases are fully automated from version tags: prebuilt binaries,
  checksums, third-party notices and release notes.
- `main` is protected; changes land through pull requests with green CI.
- Dependencies are kept up to date by Dependabot; code is scanned by
  CodeQL.
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

Binary releases also contain code from third-party Rust crates; their
licenses are listed in `THIRD-PARTY-NOTICES` inside each release archive.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally
submitted for inclusion in the work by you, as defined in the Apache-2.0
license, shall be dual licensed as above, without any additional terms or
conditions.
