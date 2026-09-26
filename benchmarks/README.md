# Benchmarks

Compares lasrs-cpp with other LAS/LAZ libraries on one LAZ file:

| Library | How it is measured |
|---|---|
| lasrs-cpp | `Reader::fill_points` / `Writer::write_points`, parallel and single-threaded |
| LASzip | `laszip_read_point` loop (the codec behind liblas, LAStools, CloudCompare) |
| laz-perf | `lazperf::reader` / `lazperf::writer` on raw records |
| PDAL | `readers.las` streaming into a counter (uses laz-perf), default and all threads |
| laspy + lazrs | `laspy.read` / `write` with the `lazrs` backend, parallel and single-threaded |

Each case is run several times and the best time is reported, as a
Markdown table ready for the main README.

## Running

The competitors come from conda-forge through [pixi](https://pixi.sh); the
environment lives in `benchmarks/.pixi` and does not affect the library
build.

```
cd benchmarks
pixi run bench                                   # uses ../test_data/reel_...laz
pixi run bench path/to/file.laz 5                # other file, 5 runs
```

On Windows run it from a "Developer PowerShell for VS" so CMake finds MSVC.

Output files are written to `output/` next to the input file and removed
afterwards.
