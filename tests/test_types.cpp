// SPDX-License-Identifier: MIT OR Apache-2.0
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "common.hpp"

using Catch::Matchers::ContainsSubstring;

TEST_CASE("headers match the linked library")
{
    CHECK(las::abi_compatible());
}

TEST_CASE("Format::new decodes point format numbers")
{
    const las::point::Format format(3);
    CHECK(format.has_gps_time);
    CHECK(format.has_color);
    CHECK_FALSE(format.is_extended);
    CHECK(format.len() == 34);
    CHECK(format.to_u8() == 3);

    const las::point::Format extended(8);
    CHECK(extended.is_extended);
    CHECK(extended.has_nir);
    CHECK(extended.len() == 38);
}

TEST_CASE("Format::new rejects invalid numbers")
{
    CHECK_THROWS_AS(las::point::Format(11), las::Error);
}

TEST_CASE("Format::to_u8 rejects impossible combinations")
{
    las::point::Format format;
    format.has_nir = true;
    CHECK_THROWS_AS(format.to_u8(), las::Error);
}

TEST_CASE("Format::extend switches to an extended format")
{
    las::point::Format format(1);
    format.extend();
    CHECK(format.to_u8() == 6);
}

TEST_CASE("Transform converts between raw and scaled values")
{
    const las::Transform transform{0.01, 100.0};
    CHECK(transform.direct(150) == 101.5);
    CHECK(transform.inverse(101.5) == 150);
    CHECK_THROWS_WITH(transform.inverse(1e12), ContainsSubstring("transform"));
}

TEST_CASE("Version knows its header size")
{
    CHECK(las::Version(1, 2).header_size() == 227);
    CHECK(las::Version(1, 3).header_size() == 235);
    CHECK(las::Version(1, 4).header_size() == 375);
    CHECK(las::Version(1, 0).requires_point_data_start_signature());
    CHECK(las::Version(1, 2) < las::Version(1, 4));
}

TEST_CASE("Bounds grow and intersect")
{
    las::Bounds bounds;
    las::Point point;
    point.x = 1.0;
    point.y = 2.0;
    point.z = 3.0;
    bounds.grow(point);
    point.x = -1.0;
    bounds.grow(point);
    CHECK(bounds.min == las::Vector<double>{-1.0, 2.0, 3.0});
    CHECK(bounds.max == las::Vector<double>{1.0, 2.0, 3.0});
    CHECK(bounds.intersect(bounds));
}

TEST_CASE("Bounds::adapt snaps to the transform grid")
{
    const las::Bounds bounds{{0.123, 0.123, 0.123}, {0.987, 0.987, 0.987}};
    const las::Vector<las::Transform> transforms{{0.1, 0.0}, {0.1, 0.0}, {0.1, 0.0}};
    const auto adapted = bounds.adapt(transforms);
    CHECK(adapted.min.x == las::Transform{0.1, 0.0}.direct(1));
    CHECK(adapted.max.x == las::Transform{0.1, 0.0}.direct(10));
}

TEST_CASE("Vlr classifies CRS records")
{
    las::Vlr vlr;
    vlr.user_id = "LASF_Projection";
    vlr.record_id = 2112;
    CHECK(vlr.is_crs());
    CHECK(vlr.is_wkt_crs());
    CHECK_FALSE(vlr.is_geotiff_crs());
    vlr.record_id = 34735;
    CHECK(vlr.is_geotiff_crs());
    CHECK(vlr.len(false) == 54);
    CHECK(vlr.len(true) == 60);
}

TEST_CASE("VoxelKey navigates the octree")
{
    const auto child = las::copc::VoxelKey::ROOT.child(7);
    CHECK(child == las::copc::VoxelKey{1, 1, 1, 1});
    CHECK(child.parent() == las::copc::VoxelKey::ROOT);
    CHECK(las::copc::VoxelKey::ROOT.children()[2] == las::copc::VoxelKey{1, 0, 1, 0});
    CHECK_THROWS_AS(child.child(8), las::Error);

    const las::copc::CopcInfoVlr info{0.0, 0.0, 0.0, 10.0};
    const auto bounds = child.bounds(info);
    CHECK(bounds.min == las::Vector<double>{0.0, 0.0, 0.0});
    CHECK(bounds.max == las::Vector<double>{10.0, 10.0, 10.0});
}

TEST_CASE("Point::matches checks optional fields against a format")
{
    las::Point point;
    CHECK(point.matches(las::point::Format(0)));
    CHECK_FALSE(point.matches(las::point::Format(1)));
    point.gps_time = 1.0;
    CHECK(point.matches(las::point::Format(1)));
}
