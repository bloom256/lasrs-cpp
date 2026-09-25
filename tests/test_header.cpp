// SPDX-License-Identifier: MIT OR Apache-2.0
#include <catch2/catch_test_macros.hpp>

#include "common.hpp"

TEST_CASE("Default header matches las-rs defaults")
{
    const las::Header header;
    CHECK(header.version() == las::Version(1, 2));
    CHECK(header.system_identifier() == "las-rs");
    CHECK(header.number_of_points() == 0);
    CHECK(header.date().has_value());
    CHECK(header.point_format() == las::point::Format(0));
}

TEST_CASE("Header copies are deep and compare equal")
{
    const las::Header original(las::Version(1, 4));
    auto copy = original;
    CHECK(copy == original);
    las::Point point;
    point.x = 5.0;
    copy.add_point(point);
    CHECK(copy.number_of_points() == 1);
    CHECK(original.number_of_points() == 0);
    CHECK_FALSE(copy == original);
}

TEST_CASE("Header::add_point updates counts and bounds")
{
    las::Header header;
    las::Point point;
    point.x = 1.0;
    point.return_number = 1;
    header.add_point(point);
    point.x = 3.0;
    header.add_point(point);
    CHECK(header.number_of_points() == 2);
    CHECK(header.number_of_points_by_return(1) == 2u);
    CHECK(header.bounds().min.x == 1.0);
    CHECK(header.bounds().max.x == 3.0);
    header.clear();
    CHECK(header.number_of_points() == 0);
}

TEST_CASE("Header::add_point_data accumulates a whole batch")
{
    auto reader = las::Reader::from_path(test::data("autzen.las"));
    las::Header header;
    header.add_point_data(reader.read_all());
    CHECK(header.number_of_points() == 106);
}

TEST_CASE("Builder configures a header")
{
    las::Builder builder(las::Version(1, 4));
    builder.point_format = las::point::Format(7);
    builder.system_identifier = "lasrs-cpp test";
    builder.generating_software = "catch2";
    builder.file_source_id = 42;
    builder.gps_time_type = las::GpsTimeType::Standard;
    builder.date = std::chrono::year_month_day{std::chrono::year{2024}, std::chrono::month{2}, std::chrono::day{29}};
    builder.guid[0] = 0xab;
    builder.transforms.x = {0.01, 500.0};
    builder.vlrs.push_back({"lasrs", 1, "a vlr", {1, 2, 3}});
    builder.evlrs.push_back({"lasrs", 2, "an evlr", {4, 5}});

    const auto header = builder.into_header();
    CHECK(header.version() == las::Version(1, 4));
    CHECK(header.point_format().to_u8() == 7);
    CHECK(header.system_identifier() == "lasrs-cpp test");
    CHECK(header.generating_software() == "catch2");
    CHECK(header.file_source_id() == 42);
    CHECK(is_standard(header.gps_time_type()));
    CHECK(header.date() == builder.date);
    CHECK(header.guid()[0] == 0xab);
    CHECK(header.transforms().x == las::Transform{0.01, 500.0});
    CHECK(header.vlrs() == builder.vlrs);
    CHECK(header.evlrs() == builder.evlrs);
    CHECK(header.all_vlrs().size() == 2);
}

TEST_CASE("Builder round trips a header, including hidden fields")
{
    auto reader = las::Reader::from_path(test::data("autzen.las"));
    const las::Builder builder(reader.header());
    CHECK(builder.point_format == reader.header().point_format());
    const auto header = builder.into_header();
    CHECK(header.number_of_points() == reader.header().number_of_points());
    CHECK(header.bounds() == reader.header().bounds());
}

TEST_CASE("Builder rejects headers las-rs cannot write")
{
    las::Builder builder(las::Version(1, 2));
    builder.point_format = las::point::Format(6);
    CHECK_THROWS_AS(builder.into_header(), las::Error);
    CHECK(builder.minimum_supported_version() == las::Version(1, 4));
}

TEST_CASE("Builder rejects invalid dates")
{
    las::Builder builder;
    builder.date = std::chrono::year_month_day{std::chrono::year{2023}, std::chrono::month{2}, std::chrono::day{30}};
    CHECK_THROWS_AS(builder.into_header(), las::Error);
}

TEST_CASE("WKT CRS can be set and read back")
{
    las::Header header(las::Version(1, 4));
    const std::string wkt = "GEOGCS[\"WGS 84\"]";
    header.set_wkt_crs(std::span(reinterpret_cast<const uint8_t *>(wkt.data()), wkt.size()));
    CHECK(header.has_wkt_crs());
    const auto bytes = header.get_wkt_crs_bytes();
    REQUIRE(bytes.has_value());
    CHECK(std::string(bytes->begin(), bytes->end()) == wkt);
    header.remove_crs_vlrs();
    CHECK_FALSE(header.has_crs_vlrs());
}

TEST_CASE("WKT CRS requires LAS 1.4")
{
    las::Header header(las::Version(1, 2));
    const std::vector<uint8_t> wkt{'x'};
    CHECK_THROWS_AS(header.set_wkt_crs(wkt), las::Error);
}
