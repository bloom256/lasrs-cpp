// SPDX-License-Identifier: MIT OR Apache-2.0
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <numeric>

#include "common.hpp"

TEST_CASE("CopcReader exposes the COPC info")
{
    const auto reader = las::CopcReader::from_path(test::data("autzen.copc.laz"));
    const auto info = reader.header().copc_info_vlr();
    REQUIRE(info.has_value());
    CHECK(info->halfsize > 0.0);
}

TEST_CASE("The COPC info VLR is recognized")
{
    const auto reader = las::CopcReader::from_path(test::data("autzen.copc.laz"));
    const auto vlrs = reader.header().vlrs();
    REQUIRE(!vlrs.empty());
    CHECK(vlrs[0].is_copc_info());
    CHECK(vlrs[0].user_id == las::copc::USER_ID);
    CHECK(vlrs[0].record_id == las::copc::CopcInfoVlr::RECORD_ID);

    las::Vlr vlr;
    CHECK_FALSE(vlr.is_copc_info());
    vlr.user_id = "copc";
    vlr.record_id = 1;
    CHECK(vlr.is_copc_info());
    vlr.user_id = "COPC";
    CHECK_FALSE(vlr.is_copc_info());
    vlr.user_id = "copc";
    vlr.record_id = las::copc::CopcHierarchyVlr::RECORD_ID;
    CHECK(vlr.is_copc_hierarchy());
}

TEST_CASE("Header::copc_hierarchy_evlr lists the same entries as CopcReader")
{
    auto reader = las::CopcReader::from_path(test::data("autzen.copc.laz"));
    const auto evlrs = reader.header().evlrs();
    CHECK(std::ranges::count_if(evlrs, &las::Vlr::is_copc_hierarchy) == 1);

    const auto hierarchy = reader.header().copc_hierarchy_evlr();
    REQUIRE(hierarchy.has_value());
    auto entries = hierarchy->iter_entries();
    auto expected = reader.hierarchy_entries();
    std::ranges::sort(entries, {}, &las::copc::Entry::key);
    std::ranges::sort(expected, {}, &las::copc::Entry::key);
    CHECK(entries == expected);

    const auto copy = *hierarchy;
    CHECK(copy.iter_entries().size() == entries.size());
}

TEST_CASE("Plain LAS files have no COPC hierarchy")
{
    const auto reader = las::Reader::from_path(test::data("autzen.las"));
    CHECK_FALSE(reader.header().copc_hierarchy_evlr().has_value());
}

TEST_CASE("Plain LAS files have no COPC info")
{
    const auto reader = las::Reader::from_path(test::data("autzen.las"));
    CHECK_FALSE(reader.header().copc_info_vlr().has_value());
}

TEST_CASE("CopcReader::query with no filter returns every point")
{
    auto reader = las::CopcReader::from_path(test::data("autzen.copc.laz"));
    const auto points = reader.query(las::LodSelection::All(), las::BoundsSelection::All());
    CHECK(points.len() == reader.header().number_of_points());
}

TEST_CASE("CopcReader hierarchy entries cover every point")
{
    auto reader = las::CopcReader::from_path(test::data("autzen.copc.laz"));
    const auto entries = reader.hierarchy_entries();
    REQUIRE_FALSE(entries.empty());
    const auto total = std::accumulate(entries.begin(), entries.end(), uint64_t{0},
                                       [](uint64_t sum, const las::copc::Entry &e) { return sum + e.point_count; });
    CHECK(total == reader.header().number_of_points());

    const auto root = reader.hierarchy_entry(las::copc::VoxelKey::ROOT);
    REQUIRE(root.has_value());
    CHECK(reader.read_entry(*root).len() == static_cast<size_t>(root->point_count));
}

TEST_CASE("CopcReader::query filters by level and bounds")
{
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

TEST_CASE("CopcReader reads from a std::istream")
{
    std::ifstream stream(test::data("autzen.copc.laz"), std::ios::binary);
    las::CopcReader reader(stream);
    CHECK(reader.query(las::LodSelection::All(), las::BoundsSelection::All()).len() ==
          reader.header().number_of_points());
}

TEST_CASE("CopcReader rejects non-COPC files")
{
    CHECK_THROWS_AS(las::CopcReader::from_path(test::data("autzen.laz")), las::Error);
}
