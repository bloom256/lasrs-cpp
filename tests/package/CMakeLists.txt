# Builds a program against an installed lasrs bundle:
#   cmake -S tests/package -B <dir> -DCMAKE_PREFIX_PATH=<install prefix>
cmake_minimum_required(VERSION 3.22)
project(lasrs_package_test LANGUAGES C CXX)

find_package(lasrs 0.1 REQUIRED)

add_executable(package_test main.cpp)
target_link_libraries(package_test PRIVATE lasrs::lasrs)

enable_testing()
add_test(NAME package_test COMMAND package_test)

add_executable(c_smoke c_smoke.c)
target_link_libraries(c_smoke PRIVATE lasrs::lasrs_ffi)
add_test(NAME c_smoke COMMAND c_smoke ${CMAKE_CURRENT_BINARY_DIR}/c_smoke.laz)
