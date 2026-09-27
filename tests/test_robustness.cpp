// SPDX-License-Identifier: MIT OR Apache-2.0
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cstring>
#include <limits>
#include <sstream>

#include "common.hpp"

// Corrupt files must produce las::Error, never an abort or a hang.
namespace
{

// LAS 1.4 header field offsets.
constexpr size_t legacy_point_count_offset = 107;
constexpr size_t start_of_first_evlr_offset = 235;
constexpr size_t point_count_offset = 247;
constexpr size_t evlr_record_length_offset = 20;

std::string las14(size_t points, bool with_evlr)
{
    las::Builder builder(las::Version(1, 4));
    if (with_evlr)
    {
        builder.evlrs.push_back({"lasrs", 1, "test", {1, 2, 3}});
    }
    std::stringstream stream;
    {
        las::Writer writer(stream, builder.into_header());
        for (size_t i = 0; i < points; ++i)
        {
            writer.write_point(las::Point());
        }
        writer.close();
    }
    return stream.str();
}

template <class T> void patch(std::string &bytes, size_t offset, T value)
{
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

template <class T> T peek(const std::string &bytes, size_t offset)
{
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

} // namespace

TEST_CASE("An EVLR longer than the file is rejected")
{
    auto bytes = las14(1, true);
    const auto evlr = peek<uint64_t>(bytes, start_of_first_evlr_offset);
    patch<uint64_t>(bytes, static_cast<size_t>(evlr) + evlr_record_length_offset, uint64_t{1} << 40);

    std::istringstream stream(bytes);
    CHECK_THROWS_AS(las::Reader(stream), las::Error);
}

TEST_CASE("An EVLR start beyond the file is rejected")
{
    auto bytes = las14(1, true);
    patch<uint64_t>(bytes, start_of_first_evlr_offset, uint64_t{1} << 40);

    std::istringstream stream(bytes);
    CHECK_THROWS_AS(las::Reader(stream), las::Error);
}

TEST_CASE("A point count larger than the data fails without a huge allocation")
{
    auto bytes = las14(3, false);
    patch<uint32_t>(bytes, legacy_point_count_offset, 0);
    patch<uint64_t>(bytes, point_count_offset, uint64_t{1} << 40);

    std::istringstream stream(bytes);
    las::Reader reader(stream);
    CHECK(reader.header().number_of_points() == uint64_t{1} << 40);
    CHECK_THROWS_AS(reader.read_all(), las::Error);
}

TEST_CASE("Reading after seeking past the last point returns nothing")
{
    const auto path = test::data(GENERATE("autzen.las", "autzen.laz"));
    auto reader = las::Reader::from_path(path);
    reader.seek(reader.header().number_of_points() + 10);
    CHECK(reader.read_points(1).is_empty());
    CHECK(reader.read_all().is_empty());
}

TEST_CASE("CopcReader::read_entry rejects entries that are not in the hierarchy")
{
    auto reader = las::CopcReader::from_path(test::data("autzen.copc.laz"));
    auto entry = reader.hierarchy_entry(las::copc::VoxelKey::ROOT);
    REQUIRE(entry.has_value());
    entry->point_count = std::numeric_limits<int32_t>::max();
    CHECK_THROWS_AS(reader.read_entry(*entry), las::Error);
}
