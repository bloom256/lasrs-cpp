// SPDX-License-Identifier: MIT OR Apache-2.0
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <sstream>

#include "common.hpp"

// Real-world data that is too large to ship; these tests are skipped unless
// the file has been placed in test_data/.
namespace
{

const std::string large_file = "reel_0005_20250821-165617_pointcloud.laz";

std::filesystem::path large_file_or_skip()
{
    const auto path = test::data(large_file);
    if (!std::filesystem::exists(path))
    {
        SKIP(large_file << " not found in test_data/");
    }
    return path;
}

} // namespace

TEST_CASE("Large file: every point is read")
{
    auto reader = las::Reader::from_path(large_file_or_skip());
    auto buffer = las::PointDataBuilder().for_header(reader.header()).build();
    uint64_t total = 0;
    while (const auto n = reader.fill_points(1'000'000, buffer))
    {
        total += n;
    }
    CHECK(total == reader.header().number_of_points());
}

TEST_CASE("Large file: parallel and sequential decompression agree")
{
    const auto path = large_file_or_skip();
    auto parallel = las::Reader::from_path(path);
    std::ifstream stream(path, std::ios::binary);
    auto sequential =
        las::Reader::with_options(stream, las::ReaderOptions().with_laz_parallelism(las::LazParallelism::No));
    const auto expected = sequential.read_points(500'000);
    CHECK(parallel.read_points(500'000).raw_bytes().size() == expected.raw_bytes().size());
    parallel.seek(0);
    CHECK(std::ranges::equal(parallel.read_points(500'000).raw_bytes(), expected.raw_bytes()));
}

TEST_CASE("Large file: LAZ round trip preserves points")
{
    auto reader = las::Reader::from_path(large_file_or_skip());
    const auto data = reader.read_points(1'000'000);
    std::stringstream stream;
    {
        las::Writer writer(stream, reader.header());
        writer.write_points(data);
        writer.close();
    }
    stream.seekg(0);
    las::Reader round_trip(stream);
    CHECK(std::ranges::equal(round_trip.read_all().raw_bytes(), data.raw_bytes()));
}
