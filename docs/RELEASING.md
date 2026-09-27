# Releasing

Releases are built and published by `.github/workflows/release.yml` when a
version tag is pushed. To release version `X.Y.Z`:

1. Run `scripts/bump-version.sh X.Y.Z`. It sets the version in
   `CMakeLists.txt`, `rust/Cargo.toml` (and `Cargo.lock`, which CI builds
   with `--locked`) and `CITATION.cff` (with today's date), points the
   README's `LASRS_VERSION` and `GIT_TAG` snippets at `vX.Y.Z`, and turns
   `## [Unreleased]` in `CHANGELOG.md` into `## [X.Y.Z] - YYYY-MM-DD` under
   a new empty `## [Unreleased]`. Review the diff.
2. Commit, push, and wait for CI to pass.
3. Tag and push the tag:

   ```
   git tag -a vX.Y.Z -m "lasrs-cpp X.Y.Z"    # or -s to sign it
   git push origin vX.Y.Z
   ```

The workflow refuses to publish if any of these versions does not match
the tag or the changelog has no section for it. It then builds the bundle for every
platform (the same `bundle.yml` CI uses), packages each as
`lasrs-cpp-vX.Y.Z-<platform>.zip` / `.tar.gz`, adds `SHA256SUMS.txt` and
build provenance attestations, and creates the GitHub release with the
changelog section as notes.
