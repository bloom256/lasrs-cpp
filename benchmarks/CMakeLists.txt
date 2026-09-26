cmake_minimum_required(VERSION 3.22)
project(lasrs_benchmarks LANGUAGES C CXX)

set(LASRS_BUILD_TESTS OFF)
set(LASRS_BUILD_EXAMPLES OFF)
add_subdirectory(.. lasrs)

find_package(PDAL CONFIG REQUIRED)
find_package(LAZPERF CONFIG REQUIRED)
find_path(LASZIP_INCLUDE_DIR laszip/laszip_api.h REQUIRED)
find_library(LASZIP_LIBRARY NAMES laszip3 laszip REQUIRED)

add_executable(lasrs_bench main.cpp)
target_compile_features(lasrs_bench PRIVATE cxx_std_20)
target_include_directories(lasrs_bench PRIVATE ${LASZIP_INCLUDE_DIR})
target_link_libraries(lasrs_bench PRIVATE lasrs::lasrs LAZPERF::lazperf pdalcpp ${LASZIP_LIBRARY})
if(WIN32)
  # laszip_api.h includes windows.h.
  target_compile_definitions(lasrs_bench PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
