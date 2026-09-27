# Uses lasrs-cpp as a subproject and installs the parent:
#   cmake -S tests/subproject -B <dir> && cmake --build <dir> && cmake --install <dir> --prefix <prefix>
# lasrs must build, add nothing to the parent's install, and link.
cmake_minimum_required(VERSION 3.22)
project(lasrs_subproject_test LANGUAGES C CXX)

add_subdirectory(../.. lasrs)

add_executable(subproject_test ../package/main.cpp)
target_link_libraries(subproject_test PRIVATE lasrs::lasrs)
add_executable(subproject_c_smoke ../package/c_smoke.c)
target_link_libraries(subproject_c_smoke PRIVATE lasrs::lasrs_ffi)
install(TARGETS subproject_test subproject_c_smoke)
