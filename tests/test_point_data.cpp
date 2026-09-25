// SPDX-License-Identifier: MIT OR Apache-2.0
#include <catch2/catch_test_macros.hpp>

#include "common.hpp"

TEST_CASE("PointData columns match decoded points")
{
    auto reader = las::Reader::from_path(test::data("autzen.laz"));
    const auto data = reader.read_all();
    const auto points = data.points();
    REQUIRE(points.size() == data.len());

    const auto x = data.x();
    const auto intensity = data.intensity();
    const auto classification = data.classification();
    const auto return_number = data.return_number();
    const auto gps_time = data.gps_time();
    const auto rgb = data.rgb();
    REQUIRE(gps_time.has_value());
    REQUIRE(rgb.has_value());
    CHECK_FALSE(data.nir().has_value());

    for (size_t i = 0; i < points.size(); ++i)
    {
        CHECK(x[i] == points[i].x);
        CHECK(intensity[i] == points[i].intensity);
        CHECK(classification[i] == static_cast<uint8_t>(points[i].classification));
        CHECK(return_number[i] == points[i].return_number);
        CHECK((*gps_time)[i] == points[i].gps_time);
        const auto &[red, green, blue] = (*rgb)[i];
        CHECK(las::Color(red, green, blue) == points[i].color);
    }
}

TEST_CASE("PointData raw columns apply the header transforms")
{
    auto reader = las::Reader::from_path(test::data("autzen.las"));
    const auto data = reader.read_points(5);
    const auto transforms = data.transforms();
    const auto x_raw = data.x_raw();
    const auto x = data.x();
    for (size_t i = 0; i < data.len(); ++i)
    {
        CHECK(transforms.x.direct(x_raw[i]) == x[i]);
    }
}

TEST_CASE("PointData raw bytes round trip through build_from_bytes")
{
    auto reader = las::Reader::from_path(test::data("autzen.las"));
    const auto data = reader.read_all();
    CHECK(data.raw_bytes().size() == data.len() * data.record_len());

    const auto copy = las::PointDataBuilder().for_header(reader.header()).build_from_bytes(data.raw_bytes());
    CHECK(copy.points() == data.points());
}

TEST_CASE("PointDataBuilder::build_from_bytes rejects partial records")
{
    const std::vector<uint8_t> bytes(21);
    CHECK_THROWS_AS(las::PointDataBuilder().with_format(las::point::Format(0)).build_from_bytes(bytes), las::Error);
}

TEST_CASE("PointDataBuilder::build_from_points encodes points")
{
    las::Point point;
    point.x = 1.0;
    point.y = 2.0;
    point.z = 3.0;
    point.gps_time = 42.0;
    const auto data = las::PointDataBuilder().with_format(las::point::Format(1)).build_from_points({point, point});
    CHECK(data.len() == 2);
    CHECK(data.points() == std::vector<las::Point>{point, point});
}

TEST_CASE("PointData::resize_for exposes a writable slab")
{
    auto reader = las::Reader::from_path(test::data("autzen.las"));
    const auto source = reader.read_points(3);
    auto target = las::PointDataBuilder().for_header(reader.header()).build();
    const auto slab = target.resize_for(3);
    REQUIRE(slab.size() == source.raw_bytes().size());
    std::ranges::copy(source.raw_bytes(), slab.begin());
    CHECK(target.points() == source.points());
}

TEST_CASE("PointData copies are independent")
{
    auto reader = las::Reader::from_path(test::data("autzen.las"));
    const auto original = reader.read_points(3);
    auto copy = original;
    copy.resize_for(1);
    CHECK(original.len() == 3);
    CHECK(copy.len() == 1);
}
