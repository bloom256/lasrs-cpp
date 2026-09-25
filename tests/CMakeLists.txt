set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.16.0
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Catch2)

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
