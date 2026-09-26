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
#include <memory>
#include <optional>
#include <pdal/PointTable.hpp>
#include <pdal/filters/StreamCallbackFilter.hpp>
#include <pdal/io/BufferReader.hpp>
#include <pdal/io/LasReader.hpp>
#include <pdal/io/LasWriter.hpp>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{

using Run = std::function<uint64_t()>;

// `prepare` loads the benchmark's input outside the timer and returns the
// timed action; the input is freed when the benchmark is done, so only one
// copy of the point cloud is in memory at a time.
struct Benchmark
{
    std::string library;
    std::string operation;
    std::function<Run()> prepare;
    std::optional<std::filesystem::path> written_file;
};

std::function<Run()> without_input(Run run)
{
    return [run] { return run; };
}

uint64_t count_points(const std::filesystem::path &path)
{
    auto reader = las::Reader::from_path(path);
    auto points = las::PointDataBuilder().for_header(reader.header()).build();
    uint64_t total = 0;
    while (const auto n = reader.fill_points(1'000'000, points))
    {
        total += n;
    }
    return total;
}

void check_written_file(const Benchmark &benchmark, uint64_t expected_points)
{
    if (!benchmark.written_file)
    {
        return;
    }
    const bool compressed = las::Reader::from_path(*benchmark.written_file).header().point_format().is_compressed;
    if (!compressed || count_points(*benchmark.written_file) != expected_points)
    {
        throw std::runtime_error(benchmark.library + " wrote an invalid LAZ file");
    }
}

double best_of(int runs, const Benchmark &benchmark, uint64_t expected_points)
{
    double best = std::numeric_limits<double>::max();
    Run run = benchmark.prepare();
    for (int i = 0; i < runs; ++i)
    {
        const auto start = std::chrono::steady_clock::now();
        const auto points = run();
        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
        if (points != expected_points)
        {
            throw std::runtime_error(benchmark.library + " processed " + std::to_string(points) + " of " +
                                     std::to_string(expected_points) + " points");
        }
        best = std::min(best, elapsed.count());
    }
    run = nullptr;
    check_written_file(benchmark, expected_points);
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

std::shared_ptr<const las::PointData> load_points(const std::filesystem::path &path)
{
    return std::make_shared<const las::PointData>(las::Reader::from_path(path).read_all());
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

// Points in LASzip's own representation, read once outside the timer. The
// reader stays open because the header points into its memory.
class LaszipPoints
{
  public:
    explicit LaszipPoints(const std::filesystem::path &path)
    {
        laszip_create(&reader_);
        laszip_BOOL is_compressed = 0;
        check_laszip(laszip_open_reader(reader_, path.string().c_str(), &is_compressed), reader_);
        check_laszip(laszip_get_header_pointer(reader_, &header_), reader_);
        laszip_point *point = nullptr;
        check_laszip(laszip_get_point_pointer(reader_, &point), reader_);
        const uint64_t count = header_->number_of_point_records ? header_->number_of_point_records
                                                                : header_->extended_number_of_point_records;
        points_.reserve(count);
        extra_bytes_.resize(count * point->num_extra_bytes);
        for (uint64_t i = 0; i < count; ++i)
        {
            check_laszip(laszip_read_point(reader_), reader_);
            points_.push_back(*point);
            // laszip_point only points at the reader's extra bytes buffer.
            auto *extra = extra_bytes_.data() + i * point->num_extra_bytes;
            std::copy_n(point->extra_bytes, point->num_extra_bytes, extra);
            points_.back().extra_bytes = extra;
        }
    }

    LaszipPoints(const LaszipPoints &) = delete;
    LaszipPoints &operator=(const LaszipPoints &) = delete;

    ~LaszipPoints()
    {
        laszip_close_reader(reader_);
        laszip_destroy(reader_);
    }

    uint64_t write(const std::filesystem::path &path) const
    {
        laszip_POINTER writer = nullptr;
        laszip_create(&writer);
        check_laszip(laszip_set_header(writer, header_), writer);
        check_laszip(laszip_open_writer(writer, path.string().c_str(), 1), writer);
        for (const auto &point : points_)
        {
            check_laszip(laszip_set_point(writer, &point), writer);
            check_laszip(laszip_write_point(writer), writer);
        }
        check_laszip(laszip_close_writer(writer), writer);
        laszip_destroy(writer);
        return points_.size();
    }

  private:
    laszip_POINTER reader_ = nullptr;
    laszip_header *header_ = nullptr;
    std::vector<laszip_point> points_;
    std::vector<laszip_U8> extra_bytes_;
};

// Points in a PDAL PointView, loaded once outside the timer.
class PdalPoints
{
  public:
    explicit PdalPoints(const std::filesystem::path &path)
    {
        pdal::Options options;
        options.add("filename", path.string());
        reader_.setOptions(options);
        reader_.prepare(table_);
        view_ = *reader_.execute(table_).begin();
    }

    uint64_t write(const std::filesystem::path &path)
    {
        pdal::BufferReader buffer;
        buffer.addView(view_);
        pdal::Options options;
        options.add("filename", path.string());
        options.add("compression", true);
        options.add("forward", "all");
        pdal::LasWriter writer;
        writer.setOptions(options);
        writer.setInput(buffer);
        writer.prepare(table_);
        writer.execute(table_);
        return view_->size();
    }

  private:
    pdal::PointTable table_;
    pdal::LasReader reader_;
    pdal::PointViewPtr view_;
};

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
        const auto header = las::Reader::from_path(path).header();
        const auto count = header.number_of_points();
        const auto parallel = std::to_string(threads) + " threads";

        const auto lasrs_writer = [&](las::LazParallelism parallelism) {
            return [&, parallelism]() -> Run {
                auto points = load_points(path);
                return [&, points, parallelism] { return lasrs_write(output, header, *points, parallelism); };
            };
        };

        const std::vector<Benchmark> benchmarks{
            {"lasrs-cpp", "read, " + parallel,
             without_input([&] { return lasrs_read(path, las::LazParallelism::Yes); })},
            {"lasrs-cpp", "read, 1 thread", without_input([&] { return lasrs_read(path, las::LazParallelism::No); })},
            {"LASzip", "read, 1 thread", without_input([&] { return laszip_read(path); })},
            {"laz-perf", "read, 1 thread", without_input([&] { return lazperf_read(path); })},
            {"PDAL", "read, 7 threads (default)", without_input([&] { return pdal_read(path, 7); })},
            {"PDAL", "read, " + parallel, without_input([&] { return pdal_read(path, threads); })},
            {"lasrs-cpp", "write, " + parallel, lasrs_writer(las::LazParallelism::Yes), output},
            {"lasrs-cpp", "write, 1 thread", lasrs_writer(las::LazParallelism::No), output},
            {"laz-perf", "write, 1 thread",
             [&]() -> Run {
                 auto points = load_points(path);
                 return [&, points] { return lazperf_write(output, header, *points); };
             },
             output},
            {"LASzip", "write, 1 thread",
             [&]() -> Run {
                 auto points = std::make_shared<const LaszipPoints>(path);
                 return [&, points] { return points->write(output); };
             },
             output},
            {"PDAL", "write, 1 thread",
             [&]() -> Run {
                 auto points = std::make_shared<PdalPoints>(path);
                 return [&, points] { return points->write(output); };
             },
             output},
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
