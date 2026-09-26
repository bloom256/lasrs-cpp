cmake_minimum_required(VERSION 3.22)

# Standalone: tests an installed bundle instead of the in-tree build, e.g.
#   cmake -S tests -B build/tests -DCMAKE_PREFIX_PATH=<bundle> -DLASRS_TEST_DATA_DIR=<sample files>
if(NOT TARGET lasrs::lasrs)
  project(lasrs_tests LANGUAGES C CXX)
  find_package(lasrs REQUIRED)
  enable_testing()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(Catch2
  URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.16.0.tar.gz
  URL_HASH SHA256=0957cae5821b17ce07f0833aaa52b5137643a8382203221f363a8303c109af34
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(Catch2)

set(LASRS_TEST_DATA_DIR "" CACHE PATH "Directory with the las crate's sample files; found through cargo if empty")
if(LASRS_TEST_DATA_DIR)
  set(test_data_dir ${LASRS_TEST_DATA_DIR})
else()
  # The las crate ships its test files; copy them into test_data/ (gitignored).
  execute_process(
    COMMAND ${Rust_CARGO_CACHED} metadata --format-version 1 --locked
            --manifest-path ${PROJECT_SOURCE_DIR}/rust/Cargo.toml
    OUTPUT_VARIABLE cargo_metadata
    COMMAND_ERROR_IS_FATAL ANY)
  string(JSON package_count LENGTH "${cargo_metadata}" packages)
  math(EXPR last_package "${package_count} - 1")
  foreach(i RANGE ${last_package})
    string(JSON package_name GET "${cargo_metadata}" packages ${i} name)
    if(package_name STREQUAL "las")
      string(JSON las_manifest GET "${cargo_metadata}" packages ${i} manifest_path)
    endif()
  endforeach()
  cmake_path(GET las_manifest PARENT_PATH las_dir)

  set(test_data_dir ${PROJECT_SOURCE_DIR}/test_data)
  file(COPY ${las_dir}/tests/data/ DESTINATION ${test_data_dir})
endif()
file(MAKE_DIRECTORY ${test_data_dir}/output)

add_executable(lasrs_tests
  test_copc.cpp
  test_header.cpp
  test_large.cpp
  test_point_data.cpp
  test_reader.cpp
  test_types.cpp
  test_writer.cpp)
target_link_libraries(lasrs_tests PRIVATE lasrs::lasrs Catch2::Catch2WithMain)
target_compile_definitions(lasrs_tests PRIVATE
  LASRS_TEST_DATA_DIR="${test_data_dir}"
  LASRS_TEST_OUTPUT_DIR="${test_data_dir}/output")

include(Catch)
catch_discover_tests(lasrs_tests)
