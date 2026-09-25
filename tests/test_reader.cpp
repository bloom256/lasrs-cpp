// SPDX-License-Identifier: MIT OR Apache-2.0
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <fstream>

#include "common.hpp"

using Catch::Matchers::ContainsSubstring;

TEST_CASE("Reader reads the autzen header") {
    const auto reader = las::Reader::from_path(test::data("autzen.las"));
    const auto& header = reader.header();
    CHECK(header.number_of_points() == 106);
    CHECK(header.version() == las::Version(1, 2));
    CHECK(header.point_format().to_u8() == 1);
    CHECK(header.number_of_points_by_return(1).has_value());
}

TEST_CASE("Parallel and sequential LAZ decompression agree") {
    std::ifstream stream(test::data("extrabytes.laz"), std::ios::binary);
    const auto options = las::ReaderOptions().with_laz_parallelism(las::LazParallelism::No);
    auto sequential = las::Reader::with_options(stream, options);
    CHECK(sequential.read_all().points() == test::read_all_points(test::data("extrabytes.laz")));
}

TEST_CASE("Reader::read_points reads in chunks") {
    const auto path = test::data(GENERATE("autzen.las", "autzen.laz", "autzen.copc.laz"));
    const auto expected = test::read_all_points(path);

    auto reader = las::Reader::from_path(path);
    std::vector<las::Point> points;
    for (auto chunk = reader.read_points(7); !chunk.is_empty(); chunk = reader.read_points(7)) {
        const auto chunk_points = chunk.points();
        points.insert(points.end(), chunk_points.begin(), chunk_points.end());
    }
    CHECK(points == expected);
}

TEST_CASE("Reader::fill_points reuses a buffer") {
    const auto path = test::data(GENERATE("autzen.las", "autzen.laz"));
    const auto expected = test::read_all_points(path);

    auto reader = las::Reader::from_path(path);
    auto buffer = las::PointDataBuilder().for_header(reader.header()).build();
    std::vector<las::Point> points;
    while (reader.fill_points(7, buffer) != 0) {
        const auto chunk_points = buffer.points();
        points.insert(points.end(), chunk_points.begin(), chunk_points.end());
    }
    CHECK(points == expected);
}

TEST_CASE("Reader::seek positions on a point") {
    const auto path = test::data(GENERATE("autzen.las", "autzen.laz"));
    auto reader = las::Reader::from_path(path);
    const auto last = reader.header().number_of_points() - 1;
    reader.seek(last);
    CHECK(reader.read_points(1).len() == 1);
    reader.seek(last + 1);
    CHECK(reader.read_points(1).is_empty());
}

TEST_CASE("Reader reads from a std::istream") {
    const auto path = test::data(GENERATE("autzen.las", "autzen.laz"));
    std::ifstream stream(path, std::ios::binary);
    las::Reader reader(stream);
    CHECK(reader.read_all().points() == test::read_all_points(path));
}

TEST_CASE("Reader reports missing files") {
    CHECK_THROWS_AS(las::Reader::from_path(test::data("does-not-exist.las")), las::Error);
}

TEST_CASE("Reader reports garbage input") {
    std::istringstream stream("definitely not a las file");
    CHECK_THROWS_AS(las::Reader(stream), las::Error);
}

TEST_CASE("Reader exposes extra bytes") {
    auto reader = las::Reader::from_path(test::data("extrabytes.laz"));
    const auto extra_bytes = reader.header().point_format().extra_bytes;
    REQUIRE(extra_bytes > 0);
    const auto points = reader.read_points(3).points();
    REQUIRE(points.size() == 3);
    for (const auto& point : points) {
        CHECK(point.extra_bytes.size() == extra_bytes);
    }
}

TEST_CASE("Reader exposes the CRS") {
    const auto reader = las::Reader::from_path(test::data("32-1-472-150-76.laz"));
    CHECK(reader.header().has_crs_vlrs());
}
