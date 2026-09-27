// SPDX-License-Identifier: MIT OR Apache-2.0
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <fstream>
#include <numeric>
#include <sstream>

#include "common.hpp"

namespace
{

las::Header header_for(uint8_t format_number, uint16_t extra_bytes, bool compress)
{
    las::Builder builder(las::Version(1, 4));
    builder.point_format = las::point::Format(format_number);
    builder.point_format.extra_bytes = extra_bytes;
    builder.point_format.is_compressed = compress;
    return builder.into_header();
}

// A point with every field the format has set to a value that survives the
// raw encoding exactly.
las::Point full_point(const las::point::Format &format, int i)
{
    las::Point p;
    p.x = 1.5 + i;
    p.y = 2.25 - i;
    p.z = 3.125 * i;
    p.intensity = static_cast<uint16_t>(100 + i);
    p.return_number = 2;
    p.number_of_returns = 3;
    p.scan_direction = las::point::ScanDirection::LeftToRight;
    p.is_edge_of_flight_line = true;
    p.classification = las::point::Classification::Building;
    p.is_synthetic = true;
    p.is_key_point = i % 2 == 0;
    p.is_withheld = true;
    p.user_data = 7;
    p.point_source_id = 42;
    if (format.is_extended)
    {
        p.is_overlap = true;
        p.scanner_channel = 2;
    }
    else
    {
        p.scan_angle = -15.0f;
    }
    if (format.has_gps_time)
    {
        p.gps_time = 123.5 + i;
    }
    if (format.has_color)
    {
        p.color = las::Color(1, 2, static_cast<uint16_t>(i));
    }
    if (format.has_nir)
    {
        p.nir = 4;
    }
    if (format.has_waveform)
    {
        p.waveform = las::raw::point::Waveform{1, 2, 3, 0.5f, 1.0f, 2.0f, 3.0f};
    }
    for (uint16_t b = 0; b < format.extra_bytes; ++b)
    {
        p.extra_bytes.push_back(static_cast<uint8_t>(b + i));
    }
    return p;
}

std::vector<las::Point> full_points(const las::Header &header)
{
    std::vector<las::Point> points;
    for (int i = 0; i < 10; ++i)
    {
        points.push_back(full_point(header.point_format(), i));
    }
    return points;
}

std::string write_one_by_one(const las::Header &header, const std::vector<las::Point> &points)
{
    std::stringstream stream;
    las::Writer writer(stream, header);
    for (const auto &point : points)
    {
        writer.write_point(point);
    }
    writer.close();
    return stream.str();
}

} // namespace

TEST_CASE("Every point format round trips every field")
{
    const auto format = static_cast<uint8_t>(GENERATE(range(0, 11)));
    const uint16_t extra_bytes = GENERATE(0, 3);
    const bool compress = GENERATE(false, true);
    CAPTURE(format, extra_bytes, compress);
    const auto header = header_for(format, extra_bytes, compress);
    const auto points = full_points(header);

    std::istringstream stream(write_one_by_one(header, points));
    las::Reader reader(stream);
    CHECK(reader.header().point_format() == header.point_format());
    CHECK(reader.header().number_of_points() == points.size());
    CHECK(reader.read_all().points() == points);
}

TEST_CASE("write_point and write_points produce the same file")
{
    const auto format = static_cast<uint8_t>(GENERATE(1, 3, 6, 8));
    const bool compress = GENERATE(false, true);
    CAPTURE(format, compress);
    const auto header = header_for(format, 2, compress);
    const auto points = full_points(header);

    std::stringstream batched;
    {
        las::Writer writer(batched, header);
        writer.write_points(las::PointDataBuilder().for_header(header).build_from_points(points));
        writer.close();
    }
    CHECK(batched.str() == write_one_by_one(header, points));
}

TEST_CASE("Files without points round trip")
{
    const bool compress = GENERATE(false, true);
    const auto header = header_for(3, 0, compress);

    std::istringstream stream(write_one_by_one(header, {}));
    las::Reader reader(stream);
    CHECK(reader.header().number_of_points() == 0);
    CHECK(reader.read_all().is_empty());
    CHECK(reader.read_points(10).is_empty());
}

TEST_CASE("EVLRs larger than a VLR can hold round trip")
{
    const bool compress = GENERATE(false, true);
    las::Builder builder(header_for(6, 0, compress));
    std::vector<uint8_t> data(70000);
    std::iota(data.begin(), data.end(), uint8_t{0});
    builder.evlrs.push_back({"lasrs", 7, "large", data});
    const auto header = builder.into_header();
    const auto points = full_points(header);

    std::istringstream stream(write_one_by_one(header, points));
    las::Reader reader(stream);
    REQUIRE(reader.header().evlrs().size() == 1);
    CHECK(reader.header().evlrs()[0] == las::Vlr{"lasrs", 7, "large", data});
    CHECK(reader.read_all().points() == points);
}

TEST_CASE("Reader::seek lands on the requested point")
{
    const auto path = test::data(GENERATE("autzen.las", "autzen.laz"));
    const bool from_stream = GENERATE(false, true);
    CAPTURE(path, from_stream);
    const auto all = test::read_all_points(path);
    REQUIRE(all.size() > 3);

    std::ifstream file(path, std::ios::binary);
    auto reader = from_stream ? las::Reader(file) : las::Reader::from_path(path);
    for (const size_t index : {all.size() - 1, size_t{0}, all.size() / 2, size_t{1}})
    {
        reader.seek(index);
        const auto points = reader.read_points(2).points();
        REQUIRE(!points.empty());
        CHECK(points.front() == all[index]);
    }
}

TEST_CASE("CopcReader selects levels of detail")
{
    auto reader = las::CopcReader::from_path(test::data("autzen.copc.laz"));
    const auto entries = reader.hierarchy_entries();
    const auto points_in_levels = [&](int32_t min, int32_t max) {
        uint64_t sum = 0;
        for (const auto &entry : entries)
        {
            if (entry.key.l >= min && entry.key.l < max)
            {
                sum += static_cast<uint64_t>(entry.point_count);
            }
        }
        return sum;
    };
    const auto all = las::BoundsSelection::All();

    CHECK(reader.query(las::LodSelection::LevelMinMax(0, 2), all).len() == points_in_levels(0, 2));
    CHECK(reader.query(las::LodSelection::LevelMinMax(1, 3), all).len() == points_in_levels(1, 3));
    CHECK(reader.query(las::LodSelection::Resolution(1e9), all).len() == points_in_levels(0, 1));
    CHECK(reader.query(las::LodSelection::Resolution(1e-9), all).len() == reader.header().number_of_points());
    CHECK_THROWS_AS(reader.query(las::LodSelection::Resolution(0.0), all), las::Error);
    CHECK_THROWS_AS(reader.query(las::LodSelection::Resolution(-1.0), all), las::Error);
}

TEST_CASE("CopcReader::hierarchy_entry is empty for missing keys")
{
    auto reader = las::CopcReader::from_path(test::data("autzen.copc.laz"));
    CHECK_FALSE(reader.hierarchy_entry(las::copc::VoxelKey{20, 0, 0, 0}).has_value());
    CHECK_FALSE(reader.hierarchy_entry(las::copc::VoxelKey{-1, 0, 0, 0}).has_value());
}
