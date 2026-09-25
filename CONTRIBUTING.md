# Contributing

Thanks for your interest in lasrs-cpp.

## Before you start

- Read [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), especially the
  performance and FFI rules.
- For larger changes, open an issue first to discuss the design.

## Development setup

See [docs/BUILDING.md](docs/BUILDING.md).

## Guidelines

- Keep the FFI surface small and batch-oriented. No per-point exported
  functions.
- Every exported Rust function must be panic-safe (`catch_unwind`) and
  return a `lasrs_status`.
- After changing exported functions, regenerate `include/lasrs/lasrs.h`
  with cbindgen and commit it.
- Add tests for new functionality (C++ tests in `tests/`, Rust unit tests
  in the shim crate where useful).
- Format code: `cargo fmt` for Rust, `clang-format` for C++.
- Use 7-bit ASCII only in source files and documentation.
- Start new source files with the SPDX header:

  ```
  // SPDX-License-Identifier: MIT OR Apache-2.0
  ```

- New Rust dependencies must pass `cargo deny check licenses`.

## Pull requests

1. Fork and create a branch.
2. Make sure `cmake --build` and `ctest` pass locally.
3. Update [CHANGELOG.md](CHANGELOG.md) under "Unreleased".
4. Open the pull request with a short description of what and why.

## License

Unless you explicitly state otherwise, any contribution intentionally
submitted for inclusion in the work by you, as defined in the Apache-2.0
license, shall be dual licensed as MIT OR Apache-2.0, without any
additional terms or conditions.
