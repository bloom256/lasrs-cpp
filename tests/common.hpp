// SPDX-License-Identifier: MIT OR Apache-2.0
#pragma once

#include <filesystem>
#include <lasrs/lasrs.hpp>
#include <string>

namespace test
{

inline std::filesystem::path data(const std::string &name)
{
    return std::filesystem::path(LASRS_TEST_DATA_DIR) / name;
}

inline std::filesystem::path output(const std::string &name)
{
    return std::filesystem::path(LASRS_TEST_OUTPUT_DIR) / name;
}

inline std::vector<las::Point> read_all_points(const std::filesystem::path &path)
{
    return las::Reader::from_path(path).read_all().points();
}

} // namespace test
