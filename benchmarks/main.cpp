// SPDX-License-Identifier: MIT OR Apache-2.0
//
// Reads and writes one LAZ file with several libraries and prints a Markdown
// table: lasrs_bench <file.laz> [runs]

#include <laszip/laszip_api.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <lasrs/lasrs.hpp>
#include <lazperf/readers.hpp>
#include <lazperf/writers.hpp>
#include <limits>
#include <pdal/PointTable.hpp>
#include <pdal/filters/StreamCallbackFilter.hpp>
#include <pdal/io/LasReader.hpp>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{

struct Benchmark
{
    std::string library;
    std::string operation;
    std::function<uint64_t()> run;
};

double best_of(int runs, const Benchmark &benchmark, uint64_t expected_points)
{
    double best = std::numeric_limits<double>::max();
    for (int i = 0; i < runs; ++i)
    {
        const auto start = std::chrono::steady_clock::now();
        const auto points = benchmark.run();
        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
        if (points != expected_points)
        {
            throw std::runtime_error(benchmark.library + " processed " + std::to_string(points) + " of " +
                                     std::to_string(expected_points) + " points");
        }
        best = std::min(best, elapsed.count());
    }
    return best;
}

uint64_t lasrs_read(const std::filesystem::path &path, las::LazParallelism parallelism)
{
    std::ifstream stream(path, std::ios::binary);
    auto reader = las::Reader::with_options(stream, las::ReaderOptions().with_laz_parallelism(parallelism));
    auto points = las::PointDataBuilder().for_header(reader.header()).build();
    uint64_t total = 0;
    while (const auto n = reader.fill_points(1'000'000, points))
    {
        total += n;
    }
    return total;
}

void check_laszip(laszip_I32 status, laszip_POINTER laszip)
{
    if (status != 0)
    {
        laszip_CHAR *message = nullptr;
        laszip_get_error(laszip, &message);
        throw std::runtime_error(std::string("LASzip: ") + (message ? message : "unknown error"));
    }
}

uint64_t laszip_read(const std::filesystem::path &path)
{
    laszip_POINTER laszip = nullptr;
    laszip_create(&laszip);
    laszip_BOOL is_compressed = 0;
    check_laszip(laszip_open_reader(laszip, path.string().c_str(), &is_compressed), laszip);
    laszip_header *header = nullptr;
    check_laszip(laszip_get_header_pointer(laszip, &header), laszip);
    const uint64_t count =
        header->number_of_point_records ? header->number_of_point_records : header->extended_number_of_point_records;
    for (uint64_t i = 0; i < count; ++i)
    {
        check_laszip(laszip_read_point(laszip), laszip);
    }
    laszip_close_reader(laszip);
    laszip_destroy(laszip);
    return count;
}

uint64_t lazperf_read(const std::filesystem::path &path)
{
    lazperf::reader::named_file file(path.string());
    std::vector<char> record(file.header().point_record_length);
    const uint64_t count = file.pointCount();
    for (uint64_t i = 0; i < count; ++i)
    {
        file.readPoint(record.data());
    }
    return count;
}

uint64_t pdal_read(const std::filesystem::path &path, unsigned threads)
{
    pdal::Options options;
    options.add("filename", path.string());
    options.add("threads", threads);
    pdal::LasReader reader;
    reader.setOptions(options);

    uint64_t count = 0;
    pdal::StreamCallbackFilter counter;
    counter.setCallback([&count](pdal::PointRef &) {
        ++count;
        return true;
    });
    counter.setInput(reader);

    pdal::FixedPointTable table(100'000);
    counter.prepare(table);
    counter.execute(table);
    return count;
}

uint64_t lasrs_write(const std::filesystem::path &path, const las::Header &header, const las::PointData &points,
                     las::LazParallelism parallelism)
{
    las::Builder builder(header);
    builder.point_format.is_compressed = true;
    std::ofstream stream(path, std::ios::binary);
    auto writer = las::Writer::with_options(stream, builder.into_header(),
                                            las::WriterOptions().with_laz_parallelism(parallelism));
    writer.write_points(points);
    writer.close();
    return points.len();
}

uint64_t lazperf_write(const std::filesystem::path &path, const las::Header &header, const las::PointData &points)
{
    const auto transforms = header.transforms();
    lazperf::writer::named_file::config config({transforms.x.scale, transforms.y.scale, transforms.z.scale},
                                               {transforms.x.offset, transforms.y.offset, transforms.z.offset});
    config.pdrf = header.point_format().to_u8();
    config.minor_version = header.version().minor;
    config.extra_bytes = header.point_format().extra_bytes;

    lazperf::writer::named_file file(path.string(), config);
    const auto bytes = points.raw_bytes();
    for (size_t offset = 0; offset < bytes.size(); offset += points.record_len())
    {
        file.writePoint(reinterpret_cast<const char *>(bytes.data() + offset));
    }
    file.close();
    return points.len();
}

void print_row(const Benchmark &benchmark, double seconds, uint64_t points)
{
    std::cout << "| " << benchmark.library << " | " << benchmark.operation << " | " << std::fixed
              << std::setprecision(2) << seconds << " | " << std::setprecision(1) << points / seconds / 1e6 << " |\n";
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: lasrs_bench <file.laz> [runs]\n";
        return 2;
    }
    const std::filesystem::path path = argv[1];
    const int runs = argc > 2 ? std::stoi(argv[2]) : 3;
    const unsigned threads = std::max(1u, std::thread::hardware_concurrency());
    const auto output = path.parent_path() / "output" / "lasrs_bench.laz";

    try
    {
        std::filesystem::create_directories(output.parent_path());
        auto reader = las::Reader::from_path(path);
        const auto header = reader.header();
        const auto points = reader.read_all();
        const auto count = header.number_of_points();
        const auto parallel = std::to_string(threads) + " threads";

        const std::vector<Benchmark> benchmarks{
            {"lasrs-cpp", "read, " + parallel, [&] { return lasrs_read(path, las::LazParallelism::Yes); }},
            {"lasrs-cpp", "read, 1 thread", [&] { return lasrs_read(path, las::LazParallelism::No); }},
            {"LASzip", "read, 1 thread", [&] { return laszip_read(path); }},
            {"laz-perf", "read, 1 thread", [&] { return lazperf_read(path); }},
            {"PDAL", "read, 7 threads (default)", [&] { return pdal_read(path, 7); }},
            {"PDAL", "read, " + parallel, [&] { return pdal_read(path, threads); }},
            {"lasrs-cpp", "write, " + parallel,
             [&] { return lasrs_write(output, header, points, las::LazParallelism::Yes); }},
            {"lasrs-cpp", "write, 1 thread",
             [&] { return lasrs_write(output, header, points, las::LazParallelism::No); }},
            {"laz-perf", "write, 1 thread", [&] { return lazperf_write(output, header, points); }},
        };

        std::cout << count << " points, " << std::filesystem::file_size(path) / (1024 * 1024) << " MB, best of " << runs
                  << " runs\n\n"
                  << "| Library | Operation | Seconds | Million points/s |\n"
                  << "|---|---|---:|---:|\n";
        for (const auto &benchmark : benchmarks)
        {
            print_row(benchmark, best_of(runs, benchmark, count), count);
        }
        std::filesystem::remove(output);
    }
    catch (const std::exception &e)
    {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
