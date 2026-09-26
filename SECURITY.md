# Security policy

## Supported versions

Security fixes go into the latest release and `main`.

## Reporting a vulnerability

Please do not open a public issue. Report vulnerabilities privately through
GitHub: on the repository's **Security** tab choose **Report a
vulnerability**. You will get an answer within a week.

Relevant reports include memory safety issues in the C ABI or the C++
wrapper, crashes or hangs on crafted LAS/LAZ/COPC files, and problems in
the build or release pipeline. Parsing and decompression happen in Rust
(las-rs, laz-rs); issues that reproduce with las-rs alone should also be
reported there.
