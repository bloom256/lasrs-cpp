# Roadmap

## M0 - Scaffold

- [x] git repo, licenses, docs
- [ ] `rust-toolchain.toml`, shim crate skeleton
- [ ] CMake + Corrosion, `CMakePresets.json`
- [ ] cbindgen config, generated `lasrs.h`
- [ ] `lasrs_abi_version()`, panic guard, `lasrs_last_error()`
- [ ] `.gitattributes` (LF line endings)
- [ ] CI workflow running from day one (see "CI / CD" below)

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
- [ ] release workflow fully automated (see "CI / CD" below)
- [ ] vcpkg overlay port

## M6 - COPC write

- [ ] depends on copc-rs writer capabilities; evaluate first

## CI / CD (GitHub Actions)

Everything is automated; nothing is built or released by hand.

### ci.yml - on every push and pull request

- [ ] build + test matrix: Windows (MSVC), Linux (GCC, Clang), macOS
      (Apple Clang, arm64); Debug and Release
- [ ] `ctest` for C++ tests, `cargo test` for the shim crate
- [ ] `cargo fmt --check`, `cargo clippy -D warnings`, `clang-format` check
- [ ] cbindgen check: committed `lasrs.h` must match the generated one
- [ ] `cargo deny check` (licenses, advisories, duplicate crates)
- [ ] sanitizers job (ASan / UBSan) on Linux
- [ ] caching: cargo registry + target dir, CMake build dir
- [ ] status badge in README (green when passing)

### release.yml - on version tag `vX.Y.Z`

- [ ] build prebuilt static libs per platform: x64-windows (/MD and /MT),
      x64-linux, arm64-linux, arm64-macos, x64-macos
- [ ] package headers, CMake package config, `THIRD-PARTY-NOTICES`
      (`cargo about`), LICENSE files into one archive per platform
- [ ] SHA256 checksums and build provenance attestation
- [ ] create GitHub Release with generated notes from CHANGELOG.md
- [ ] smoke test: consume each archive from a tiny CMake project

### Repository hygiene

- [ ] branch protection on `main`: PRs required, CI must pass
- [ ] Dependabot for GitHub Actions and Cargo dependencies
- [ ] CodeQL code scanning (C++ and Rust)
- [ ] issue templates (bug, feature), pull request template
- [ ] SECURITY.md (how to report vulnerabilities), CODE_OF_CONDUCT.md
- [ ] README badges: CI, release version, license
- [ ] semantic versioning, Keep a Changelog, signed tags

## Open questions

- Does las-rs expose parallel LAZ writing, or must the shim call laz-rs
  directly for that?
- How complete is the copc-rs writer?
- Exact API for typed point access: raw records, SoA columns, typed views,
  or all three?
- Minimum supported Rust version and C++ standard (C++17 assumed).
