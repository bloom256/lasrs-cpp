# Benchmarks

Compares lasrs-cpp with other LAS/LAZ libraries on one LAZ file:

| Library | How it is measured |
|---|---|
| lasrs-cpp | `Reader::fill_points` (1M-point batches) / `Writer::write_points`, parallel and single-threaded |
| LASzip | `laszip_read_point` / `laszip_write_point` loops (the codec behind liblas, LAStools, CloudCompare) |
| laz-perf | `lazperf::reader` / `lazperf::writer` on raw records |
| PDAL | `readers.las` streaming with 1 thread, the default 7 and all threads; `writers.las` (default settings plus `compression`, `forward=all`) from a `PointView` |
| laspy + lazrs | `chunk_iterator` (1M-point batches) / `LasData.write`, parallel and single-threaded |

## Running

The competitors come from conda-forge through [pixi](https://pixi.sh); the
environment lives in `benchmarks/.pixi` and does not affect the library
build.

```
cd benchmarks
pixi run bench                           # default data, see below
pixi run bench path/to/file.laz 5        # your own file, 5 runs
```

On Windows run it from a "Developer PowerShell for VS" so CMake finds MSVC.

Without a file, the benchmark downloads its default data once into
`test_data/`: AHN4 tile 25GN2_18 (Amsterdam), 105.6 million points, LAS
1.4 point format 8, 952 MB. AHN is public domain (CC0); the tile is served
by [GeoTiles](https://geotiles.citg.tudelft.nl), TU Delft.

## What you get

Everything goes to `test_data/output/bench/<input name>/`:

- `results.md`: the machine, the timing tables and the verification table
- one `.laz` per write benchmark, e.g. `lasrs-cpp_write--12-threads.laz`

About 7 output files of roughly the input's size are kept, so plan the
disk space accordingly.

To redraw the README chart from a results file:

```
pixi run python plot_results.py ../test_data/output/bench/<input name>/results.md ../docs/images
```

## Method

- Each benchmark case runs in its own process, several times one after
  another; the best time is reported.
- Peak memory is how much the process grew during the timed operation.
  Write benchmarks get their input points already in memory, in each
  library's own representation, loaded before the timer starts; that input
  is not counted. LASzip streams its input in 1M-point batches instead,
  because one `laszip_point` per point would not fit in RAM.
- Every written file is decompressed and compared with the input record by
  record ("Same points as input"). Standard fields and extra bytes are
  compared separately: PDAL's default writer does not keep extra bytes.
- Results are shown as separate read and write tables, fastest first.
- The input file is read from the OS cache after the first run.
