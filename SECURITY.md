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

## Untrusted files

Corrupt files produce `las::Error`. lasrs-cpp checks the sizes a file
declares for its EVLRs and LAZ chunk table and reads points in batches, so
a bogus header does not lead to huge allocations. One known gap remains:
las-rs allocates the points of a COPC hierarchy entry from the count in
the file, and in Rust a failed allocation aborts the process. If you read
COPC files from untrusted sources, do it in a separate process.
