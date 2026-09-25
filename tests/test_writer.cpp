// SPDX-License-Identifier: MIT OR Apache-2.0
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <sstream>

#include "common.hpp"

namespace {

las::Header header_for(const las::Header& source, las::Version version, bool compressed) {
    las::Builder builder(source);
    builder.version = version;
    builder.point_format.is_compressed = compressed;
    return builder.into_header();
}

}  // namespace

TEST_CASE("Writer round trips autzen through every LAS version") {
    const auto version =
        GENERATE(las::Version(1, 0), las::Version(1, 1), las::Version(1, 2), las::Version(1, 3), las::Version(1, 4));
    auto reader = las::Reader::from_path(test::data("autzen.las"));
    const auto points = reader.read_all().points();

    std::stringstream stream;
    {
        las::Writer writer(stream, header_for(reader.header(), version, false));
        for (const auto& point : points) {
            writer.write_point(point);
        }
        writer.close();
    }
    stream.seekg(0);
    las::Reader round_trip(stream);
    CHECK(round_trip.header().version() == version);
    CHECK(round_trip.read_all().points() == points);
}

TEST_CASE("Writer::from_path picks compression from the extension") {
    const auto extension = GENERATE(std::string(".las"), std::string(".laz"));
    const auto path = test::output("from_path" + extension);
    auto reader = las::Reader::from_path(test::data("autzen.las"));
    const auto data = reader.read_all();
    {
        auto writer = las::Writer::from_path(path, reader.header());
        writer.write_points(data);
        writer.close();
    }
    auto round_trip = las::Reader::from_path(path);
    CHECK(round_trip.header().point_format().is_compressed == (extension == ".laz"));
    CHECK(round_trip.read_all().points() == data.points());
}

TEST_CASE("Writer compresses LAZ with and without parallelism") {
    const auto parallelism = GENERATE(las::LazParallelism::Yes, las::LazParallelism::No);
    auto reader = las::Reader::from_path(test::data("autzen.laz"));
    const auto data = reader.read_all();

    std::stringstream stream;
    {
        auto writer = las::Writer::with_options(stream, header_for(reader.header(), las::Version(1, 2), true),
                                                las::WriterOptions().with_laz_parallelism(parallelism));
        writer.write_points(data);
        writer.close();
    }
    stream.seekg(0);
    las::Reader round_trip(stream);
    CHECK(round_trip.read_all().points() == data.points());
}

TEST_CASE("Writer keeps extra bytes") {
    auto reader = las::Reader::from_path(test::data("extrabytes.laz"));
    const auto data = reader.read_all();
    std::stringstream stream;
    {
        las::Writer writer(stream, reader.header());
        writer.write_points(data);
        writer.close();
    }
    stream.seekg(0);
    las::Reader round_trip(stream);
    CHECK(round_trip.read_all().points() == data.points());
}

TEST_CASE("Writer updates its header as points are written") {
    std::stringstream stream;
    las::Writer writer(stream, las::Header());
    las::Point point;
    point.x = 7.0;
    writer.write_point(point);
    CHECK(writer.header().number_of_points() == 1);
    CHECK(writer.header().bounds().max.x == 7.0);
    writer.close();
}

TEST_CASE("Writer rejects points that do not match the format") {
    std::stringstream stream;
    las::Writer writer(stream, las::Header());
    las::Point point;
    point.gps_time = 1.0;
    CHECK_THROWS_AS(writer.write_point(point), las::Error);
}

TEST_CASE("Writer rejects the overlap classification") {
    std::stringstream stream;
    las::Writer writer(stream, las::Header());
    las::Point point;
    point.classification = static_cast<las::point::Classification>(12);
    CHECK_THROWS_AS(writer.write_point(point), las::Error);
}

TEST_CASE("Writer::close twice reports an error") {
    std::stringstream stream;
    las::Writer writer(stream, las::Header());
    writer.close();
    CHECK_THROWS_AS(writer.close(), las::Error);
}

TEST_CASE("Writer closes itself when destroyed") {
    std::stringstream stream;
    {
        las::Writer writer(stream, las::Header());
        writer.write_point(las::Point());
    }
    stream.seekg(0);
    las::Reader round_trip(stream);
    CHECK(round_trip.header().number_of_points() == 1);
}
