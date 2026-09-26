# Releasing

Releases are built and published by `.github/workflows/release.yml` when a
version tag is pushed. To release version `X.Y.Z`:

1. Set the version in `CMakeLists.txt` (`project(... VERSION X.Y.Z)`) and
   `rust/Cargo.toml` (`version = "X.Y.Z"`).
2. Set `LASRS_VERSION` in the README's download snippet to `vX.Y.Z`, and
   `version` / `date-released` in `CITATION.cff`.
3. In `CHANGELOG.md`, rename `## [Unreleased]` to `## [X.Y.Z] - YYYY-MM-DD`
   and add a new empty `## [Unreleased]` above it.
4. Commit, push, and wait for CI to pass.
5. Tag and push the tag:

   ```
   git tag -a vX.Y.Z -m "lasrs-cpp X.Y.Z"    # or -s to sign it
   git push origin vX.Y.Z
   ```

The workflow refuses to publish if the tag does not match both versions or
the changelog has no section for it. It then builds the bundle for every
platform (the same `bundle.yml` CI uses), packages each as
`lasrs-cpp-vX.Y.Z-<platform>.zip` / `.tar.gz`, adds `SHA256SUMS.txt` and
build provenance attestations, and creates the GitHub release with the
changelog section as notes.
