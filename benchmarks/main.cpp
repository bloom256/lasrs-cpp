// SPDX-License-Identifier: MIT OR Apache-2.0
//
// lasrs_bench <input.laz> <output dir> [runs]
//     Reads and writes the input with several libraries, keeps every written
//     file in the output directory and writes a Markdown table to stdout and
//     <output dir>/results.md.
// lasrs_bench --verify <input.laz> <output dir>
//     Checks that every .laz file in the output directory holds exactly the
//     input's point records.

#include <laszip/laszip_api.h>

#include <algorithm>
#include <cctype>
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
#include <pdal/PointTable.hpp>
#include <pdal/filters/StreamCallbackFilter.hpp>
#include <pdal/io/BufferReader.hpp>
#include <pdal/io/LasReader.hpp>
#include <pdal/io/LasWriter.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

struct Measurement
{
    uint64_t points = 0;
    double seconds = 0.0;
};

using Run = std::function<Measurement()>;

// `prepare` loads the benchmark's input outside the timer and returns the
// action to time; the input is freed when the benchmark is done, so only one
// copy of the point cloud is in memory at a time.
struct Benchmark
{
    std::string library;
    std::string operation;
    std::function<Run(const fs::path &output)> prepare;
    bool writes = false;
};

double seconds_since(Clock::time_point start)
{
    return std::chrono::duration<double>(Clock::now() - start).count();
}

template <class Action> Run timed(Action action)
{
    return [action] {
        const auto start = Clock::now();
        const uint64_t points = action();
        return Measurement{points, seconds_since(start)};
    };
}

template <class Action> std::function<Run(const fs::path &)> without_input(Action action)
{
    return [action](const fs::path &) { return timed(action); };
}

std::string file_name(const Benchmark &benchmark)
{
    std::string name = benchmark.library + "_" + benchmark.operation;
    std::ranges::replace_if(name, [](char c) { return !std::isalnum(static_cast<unsigned char>(c)); }, '-');
    return name + ".laz";
}

Measurement best_of(int runs, const Benchmark &benchmark, const fs::path &output, uint64_t expected_points)
{
    Run run = benchmark.prepare(output);
    Measurement best{0, std::numeric_limits<double>::max()};
    for (int i = 0; i < runs; ++i)
    {
        const auto measurement = run();
        if (measurement.points != expected_points)
        {
            throw std::runtime_error(benchmark.library + " processed " + std::to_string(measurement.points) + " of " +
                                     std::to_string(expected_points) + " points");
        }
        best = measurement.seconds < best.seconds ? measurement : best;
    }
    return best;
}

uint64_t lasrs_read(const fs::path &path, las::LazParallelism parallelism)
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

// Owns a LASzip reader positioned at the first point.
class LaszipReader
{
  public:
    explicit LaszipReader(const fs::path &path)
    {
        laszip_create(&reader_);
        laszip_BOOL is_compressed = 0;
        check_laszip(laszip_open_reader(reader_, path.string().c_str(), &is_compressed), reader_);
        check_laszip(laszip_get_header_pointer(reader_, &header_), reader_);
        check_laszip(laszip_get_point_pointer(reader_, &point_), reader_);
    }

    LaszipReader(const LaszipReader &) = delete;
    LaszipReader &operator=(const LaszipReader &) = delete;

    ~LaszipReader()
    {
        laszip_close_reader(reader_);
        laszip_destroy(reader_);
    }

    uint64_t count() const
    {
        return header_->number_of_point_records ? header_->number_of_point_records
                                                : header_->extended_number_of_point_records;
    }

    const laszip_header *header() const
    {
        return header_;
    }

    const laszip_point &read()
    {
        check_laszip(laszip_read_point(reader_), reader_);
        return *point_;
    }

  private:
    laszip_POINTER reader_ = nullptr;
    laszip_header *header_ = nullptr;
    laszip_point *point_ = nullptr;
};

uint64_t laszip_read(const fs::path &path)
{
    LaszipReader reader(path);
    for (uint64_t i = 0; i < reader.count(); ++i)
    {
        reader.read();
    }
    return reader.count();
}

// Reads 1M points at a time with LASzip outside the timer and times only the
// writing: a laszip_point per point for the whole file would not fit in RAM.
Measurement laszip_write(const fs::path &input, const fs::path &output)
{
    constexpr uint64_t batch_size = 1'000'000;
    LaszipReader reader(input);
    const auto extra_count =
        reader.header()->point_data_record_length - las::point::Format(reader.header()->point_data_format).len();
    std::vector<laszip_point> batch;
    std::vector<laszip_U8> extra_bytes(batch_size * extra_count);

    auto start = Clock::now();
    laszip_POINTER writer = nullptr;
    laszip_create(&writer);
    check_laszip(laszip_set_header(writer, reader.header()), writer);
    check_laszip(laszip_open_writer(writer, output.string().c_str(), 1), writer);
    double seconds = seconds_since(start);

    for (uint64_t done = 0; done < reader.count(); done += batch.size())
    {
        batch.clear();
        for (uint64_t i = 0; i < std::min(batch_size, reader.count() - done); ++i)
        {
            batch.push_back(reader.read());
            // laszip_point only points at the reader's extra bytes buffer.
            auto *extra = extra_bytes.data() + i * extra_count;
            std::copy_n(batch.back().extra_bytes, extra_count, extra);
            batch.back().extra_bytes = extra;
        }
        start = Clock::now();
        for (const auto &point : batch)
        {
            check_laszip(laszip_set_point(writer, &point), writer);
            check_laszip(laszip_write_point(writer), writer);
        }
        seconds += seconds_since(start);
    }

    start = Clock::now();
    check_laszip(laszip_close_writer(writer), writer);
    laszip_destroy(writer);
    seconds += seconds_since(start);
    return {reader.count(), seconds};
}

uint64_t lazperf_read(const fs::path &path)
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

uint64_t lazperf_write(const fs::path &path, const las::Header &header, const las::PointData &points)
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

uint64_t pdal_read(const fs::path &path, unsigned threads)
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

// Points in a PDAL PointView, loaded outside the timer.
class PdalPoints
{
  public:
    explicit PdalPoints(const fs::path &path)
    {
        pdal::Options options;
        options.add("filename", path.string());
        reader_.setOptions(options);
        reader_.prepare(table_);
        view_ = *reader_.execute(table_).begin();
    }

    uint64_t write(const fs::path &path)
    {
        pdal::BufferReader buffer;
        buffer.addView(view_);
        pdal::Options options;
        options.add("filename", path.string());
        options.add("compression", true);
        options.add("forward", "all");
        options.add("extra_dims", "all");
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

uint64_t lasrs_write(const fs::path &path, const las::Header &header, const las::PointData &points,
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

std::shared_ptr<const las::PointData> load_points(const fs::path &path)
{
    return std::make_shared<const las::PointData>(las::Reader::from_path(path).read_all());
}

std::string row(const std::string &library, const std::string &operation, const Measurement &measurement)
{
    std::ostringstream out;
    out << "| " << library << " | " << operation << " | " << std::fixed << std::setprecision(2) << measurement.seconds
        << " | " << std::setprecision(1) << measurement.points / measurement.seconds / 1e6 << " |\n";
    return out.str();
}

int benchmark(const fs::path &input, const fs::path &output_dir, int runs)
{
    const unsigned threads = std::max(1u, std::thread::hardware_concurrency());
    const auto header = las::Reader::from_path(input).header();
    const auto count = header.number_of_points();
    const auto parallel = std::to_string(threads) + " threads";

    const auto lasrs_writer = [&](las::LazParallelism parallelism) {
        return [&, parallelism](const fs::path &output) -> Run {
            auto points = load_points(input);
            return timed(
                [&, output, points, parallelism] { return lasrs_write(output, header, *points, parallelism); });
        };
    };

    const std::vector<Benchmark> benchmarks{
        {"lasrs-cpp", "read, " + parallel, without_input([&] { return lasrs_read(input, las::LazParallelism::Yes); })},
        {"lasrs-cpp", "read, 1 thread", without_input([&] { return lasrs_read(input, las::LazParallelism::No); })},
        {"LASzip", "read, 1 thread", without_input([&] { return laszip_read(input); })},
        {"laz-perf", "read, 1 thread", without_input([&] { return lazperf_read(input); })},
        {"PDAL", "read, 7 threads (default)", without_input([&] { return pdal_read(input, 7); })},
        {"PDAL", "read, " + parallel, without_input([&] { return pdal_read(input, threads); })},
        {"lasrs-cpp", "write, " + parallel, lasrs_writer(las::LazParallelism::Yes), true},
        {"lasrs-cpp", "write, 1 thread", lasrs_writer(las::LazParallelism::No), true},
        {"laz-perf", "write, 1 thread",
         [&](const fs::path &output) -> Run {
             auto points = load_points(input);
             return timed([&, output, points] { return lazperf_write(output, header, *points); });
         },
         true},
        {"LASzip", "write, 1 thread",
         [&](const fs::path &output) -> Run { return [&, output] { return laszip_write(input, output); }; }, true},
        {"PDAL", "write, 1 thread",
         [&](const fs::path &output) -> Run {
             auto points = std::make_shared<PdalPoints>(input);
             return timed([output, points] { return points->write(output); });
         },
         true},
    };

    fs::create_directories(output_dir);
    std::ofstream results(output_dir / "results.md");
    std::ostringstream title;
    title << input.filename().string() << ": " << count << " points, point format "
          << int{header.point_format().to_u8()} << ", " << fs::file_size(input) / (1024 * 1024) << " MB, best of "
          << runs << " runs\n\n"
          << "| Library | Operation | Seconds | Million points/s |\n"
          << "|---|---|---:|---:|\n";
    std::cout << title.str();
    results << title.str();
    for (const auto &benchmark : benchmarks)
    {
        const auto output = output_dir / (benchmark.writes ? file_name(benchmark) : "unused.laz");
        const auto line = row(benchmark.library, benchmark.operation, best_of(runs, benchmark, output, count));
        std::cout << line << std::flush;
        results << line << std::flush;
    }
    return 0;
}

// Compares decompressed point records, which is stricter than comparing
// decoded fields and independent of how each writer chunks the LAZ data.
std::string compare(const fs::path &input, const fs::path &output)
{
    auto expected = las::Reader::from_path(input);
    auto actual = las::Reader::from_path(output);
    if (actual.header().point_format() != expected.header().point_format() ||
        actual.header().transforms() != expected.header().transforms())
    {
        return "no: different point format or scale/offset";
    }
    auto expected_points = las::PointDataBuilder().for_header(expected.header()).build();
    auto actual_points = las::PointDataBuilder().for_header(actual.header()).build();
    uint64_t checked = 0;
    uint64_t differing = 0;
    while (const auto n = expected.fill_points(1'000'000, expected_points))
    {
        if (actual.fill_points(n, actual_points) != n)
        {
            return "no: fewer points";
        }
        const auto a = expected_points.raw_bytes();
        const auto b = actual_points.raw_bytes();
        const auto record = expected_points.record_len();
        for (size_t offset = 0; offset < a.size(); offset += record)
        {
            differing += !std::equal(a.begin() + offset, a.begin() + offset + record, b.begin() + offset);
        }
        checked += n;
    }
    if (actual.fill_points(1, actual_points) != 0)
    {
        return "no: more points";
    }
    return differing == 0
               ? "yes"
               : "no: " + std::to_string(differing) + " of " + std::to_string(checked) + " point records differ";
}

int verify(const fs::path &input, const fs::path &output_dir)
{
    std::vector<fs::path> outputs;
    for (const auto &entry : fs::directory_iterator(output_dir))
    {
        if (entry.path().extension() == ".laz")
        {
            outputs.push_back(entry.path());
        }
    }
    std::ranges::sort(outputs);

    std::ofstream results(output_dir / "results.md", std::ios::app);
    const std::string title = "\n| Written file | Same points as input |\n|---|---|\n";
    std::cout << title;
    results << title;
    bool all_same = true;
    for (const auto &output : outputs)
    {
        const auto result = compare(input, output);
        all_same = all_same && result == "yes";
        const auto line = "| " + output.filename().string() + " | " + result + " |\n";
        std::cout << line << std::flush;
        results << line << std::flush;
    }
    return all_same ? 0 : 1;
}

} // namespace

int main(int argc, char **argv)
{
    try
    {
        const std::vector<std::string> args(argv + 1, argv + argc);
        if (args.size() == 3 && args[0] == "--verify")
        {
            return verify(args[1], args[2]);
        }
        if (args.size() == 2 || args.size() == 3)
        {
            return benchmark(args[0], args[1], args.size() == 3 ? std::stoi(args[2]) : 3);
        }
        std::cerr << "usage: lasrs_bench <input.laz> <output dir> [runs]\n"
                     "       lasrs_bench --verify <input.laz> <output dir>\n";
        return 2;
    }
    catch (const std::exception &e)
    {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
