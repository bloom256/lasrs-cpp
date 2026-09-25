// SPDX-License-Identifier: MIT OR Apache-2.0
#include <chrono>
#include <iostream>
#include <lasrs/lasrs.hpp>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: lasrs_info <file.las|file.laz>\n";
        return 2;
    }
    try {
        const auto start = std::chrono::steady_clock::now();
        auto reader = las::Reader::from_path(argv[1]);
        const auto& header = reader.header();
        const auto bounds = header.bounds();
        std::cout << "version:      " << int{header.version().major} << '.' << int{header.version().minor} << '\n'
                  << "point format: " << int{header.point_format().to_u8()}
                  << (header.point_format().is_compressed ? " (compressed)" : "") << '\n'
                  << "points:       " << header.number_of_points() << '\n'
                  << "bounds:       (" << bounds.min.x << ", " << bounds.min.y << ", " << bounds.min.z << ") - ("
                  << bounds.max.x << ", " << bounds.max.y << ", " << bounds.max.z << ")\n";

        constexpr uint64_t chunk = 1'000'000;
        auto buffer = las::PointDataBuilder().for_header(header).build();
        uint64_t total = 0;
        while (const auto n = reader.fill_points(chunk, buffer)) {
            total += n;
        }
        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
        std::cout << "read " << total << " points in " << elapsed.count() << " s\n";
    } catch (const las::Error& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
