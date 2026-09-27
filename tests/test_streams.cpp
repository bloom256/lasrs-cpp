// SPDX-License-Identifier: MIT OR Apache-2.0
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <fstream>
#include <iterator>
#include <sstream>

#include "common.hpp"

using Catch::Matchers::ContainsSubstring;

namespace
{

std::string file_bytes(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}

las::Header compressed(const las::Header &header, bool compress)
{
    las::Builder builder(header);
    builder.point_format.is_compressed = compress;
    return builder.into_header();
}

} // namespace

TEST_CASE("Reader reads LAS data that starts inside a stream")
{
    const auto path = test::data(GENERATE("autzen.las", "autzen.laz"));
    std::istringstream stream("JUNKJUNK" + file_bytes(path));
    stream.seekg(8);
    las::Reader reader(stream);
    CHECK(reader.read_all().points() == test::read_all_points(path));
}

TEST_CASE("CopcReader reads COPC data that starts inside a stream")
{
    std::istringstream stream("JUNK" + file_bytes(test::data("autzen.copc.laz")));
    stream.seekg(4);
    las::CopcReader reader(stream);
    CHECK(reader.query(las::LodSelection::All(), las::BoundsSelection::All()).len() ==
          reader.header().number_of_points());
}

TEST_CASE("Writer writes LAS data that starts inside a stream")
{
    const bool compress = GENERATE(false, true);
    auto reader = las::Reader::from_path(test::data("autzen.las"));
    const auto points = reader.read_all();

    std::stringstream stream;
    stream << "PREFIX";
    {
        las::Writer writer(stream, compressed(reader.header(), compress));
        writer.write_points(points);
        writer.close();
    }
    CHECK(stream.str().substr(0, 6) == "PREFIX");
    stream.seekg(6);
    las::Reader round_trip(stream);
    CHECK(round_trip.header().point_format().is_compressed == compress);
    CHECK(round_trip.read_all().points() == points.points());
}

TEST_CASE("Readers and writers ignore the stream's exception settings")
{
    const auto path = test::data(GENERATE("autzen.las", "autzen.laz"));
    std::ifstream input(path, std::ios::binary);
    input.exceptions(std::ios::failbit | std::ios::badbit);
    las::Reader reader(input);
    const auto points = reader.read_all();
    CHECK(points.len() == reader.header().number_of_points());

    std::stringstream output;
    output.exceptions(std::ios::failbit | std::ios::badbit);
    {
        las::Writer writer(output, compressed(reader.header(), true));
        writer.write_points(points);
        writer.close();
    }
    output.seekg(0);
    CHECK(las::Reader(output).read_all().points() == points.points());
}

TEST_CASE("Unusable streams are rejected with a clear error")
{
    std::ifstream missing(test::data("does-not-exist.las"), std::ios::binary);
    CHECK_THROWS_WITH(las::Reader(missing), ContainsSubstring("input stream is not usable"));
    CHECK_THROWS_WITH(las::CopcReader(missing), ContainsSubstring("input stream is not usable"));

    std::stringstream failed;
    failed.setstate(std::ios::failbit);
    CHECK_THROWS_WITH(las::Writer(failed, las::Header()), ContainsSubstring("output stream is not usable"));
}

TEST_CASE("Views into Writer::header stay valid while the writer lives")
{
    std::stringstream stream;
    las::Writer writer(stream, las::Header());
    const std::string_view system_identifier = writer.header().system_identifier();
    writer.write_point(las::Point());
    CHECK(system_identifier == "las-rs");
}
