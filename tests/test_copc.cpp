// SPDX-License-Identifier: MIT OR Apache-2.0
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <numeric>

#include "common.hpp"

TEST_CASE("CopcReader exposes the COPC info") {
    const auto reader = las::CopcReader::from_path(test::data("autzen.copc.laz"));
    const auto info = reader.header().copc_info_vlr();
    REQUIRE(info.has_value());
    CHECK(info->halfsize > 0.0);
}

TEST_CASE("Plain LAS files have no COPC info") {
    const auto reader = las::Reader::from_path(test::data("autzen.las"));
    CHECK_FALSE(reader.header().copc_info_vlr().has_value());
}

TEST_CASE("CopcReader::query with no filter returns every point") {
    auto reader = las::CopcReader::from_path(test::data("autzen.copc.laz"));
    const auto points = reader.query(las::LodSelection::All(), las::BoundsSelection::All());
    CHECK(points.len() == reader.header().number_of_points());
}

TEST_CASE("CopcReader hierarchy entries cover every point") {
    auto reader = las::CopcReader::from_path(test::data("autzen.copc.laz"));
    const auto entries = reader.hierarchy_entries();
    REQUIRE_FALSE(entries.empty());
    const auto total = std::accumulate(entries.begin(), entries.end(), uint64_t{0},
                                       [](uint64_t sum, const las::copc::Entry& e) { return sum + e.point_count; });
    CHECK(total == reader.header().number_of_points());

    const auto root = reader.hierarchy_entry(las::copc::VoxelKey::ROOT);
    REQUIRE(root.has_value());
    CHECK(reader.read_entry(*root).len() == static_cast<size_t>(root->point_count));
}

TEST_CASE("CopcReader::query filters by level and bounds") {
    auto reader = las::CopcReader::from_path(test::data("autzen.copc.laz"));
    const auto root = reader.hierarchy_entry(las::copc::VoxelKey::ROOT);
    REQUIRE(root.has_value());
    CHECK(reader.query(las::LodSelection::Level(0), las::BoundsSelection::All()).len() ==
          static_cast<size_t>(root->point_count));

    const auto bounds = reader.header().bounds();
    const auto everything = reader.query(las::LodSelection::All(), las::BoundsSelection::Within(bounds));
    CHECK(everything.len() == reader.header().number_of_points());

    const las::Bounds nowhere{{1e9, 1e9, 1e9}, {1e9 + 1, 1e9 + 1, 1e9 + 1}};
    CHECK(reader.query(las::LodSelection::All(), las::BoundsSelection::Within(nowhere)).is_empty());
}

TEST_CASE("CopcReader reads from a std::istream") {
    std::ifstream stream(test::data("autzen.copc.laz"), std::ios::binary);
    las::CopcReader reader(stream);
    CHECK(reader.query(las::LodSelection::All(), las::BoundsSelection::All()).len() ==
          reader.header().number_of_points());
}

TEST_CASE("CopcReader rejects non-COPC files") {
    CHECK_THROWS_AS(las::CopcReader::from_path(test::data("autzen.laz")), las::Error);
}
