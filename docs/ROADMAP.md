# Roadmap

## M0 - Scaffold

- [ ] git repo, licenses, docs
- [ ] `rust-toolchain.toml`, shim crate skeleton
- [ ] CMake + Corrosion, `CMakePresets.json`
- [ ] cbindgen config, generated `lasrs.h`
- [ ] `lasrs_abi_version()`, panic guard, `lasrs_last_error()`
- [ ] CI: build on Windows, Linux, macOS

## M1 - Read MVP

- [ ] open LAS / LAZ file, read header
- [ ] parallel read of raw point records into a caller buffer
- [ ] C++ `lasrs::Reader` with RAII and exceptions
- [ ] example: read file, print header and timing

## M2 - Full read side

- [ ] all point formats 0 - 10
- [ ] VLR / EVLR access, CRS (WKT and GeoTIFF keys)
- [ ] extra bytes descriptions
- [ ] SoA column output (x, y, z as scaled doubles, attributes)
- [ ] seek / chunked streaming for files larger than RAM
- [ ] tests against a corpus of sample files

## M3 - Write side

- [ ] LAS writer
- [ ] LAZ writer, parallel compression
- [ ] header / VLR construction from C++
- [ ] round-trip tests (read -> write -> read, byte and value checks)

## M4 - COPC read

- [ ] open COPC, read info and hierarchy
- [ ] query by bounds and by level of detail
- [ ] parallel node decoding
- [ ] (optional) HTTP range-request source

## M5 - Packaging

- [ ] CMake package config (`find_package(lasrs)`)
- [ ] release workflow: prebuilt static libs per platform
- [ ] `cargo deny` license check, `cargo about` notices
- [ ] vcpkg overlay port

## M6 - COPC write

- [ ] depends on copc-rs writer capabilities; evaluate first

## Open questions

- Does las-rs expose parallel LAZ writing, or must the shim call laz-rs
  directly for that?
- How complete is the copc-rs writer?
- Exact API for typed point access: raw records, SoA columns, typed views,
  or all three?
- Minimum supported Rust version and C++ standard (C++17 assumed).
