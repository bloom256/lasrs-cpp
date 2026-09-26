cmake_minimum_required(VERSION 3.22)

project(lasrs-cpp
  VERSION 0.1.0
  DESCRIPTION "Fast LAS/LAZ/COPC for C++, powered by las-rs"
  HOMEPAGE_URL "https://github.com/bloom256/lasrs-cpp"
  LANGUAGES C CXX)

option(LASRS_BUILD_TESTS "Build lasrs-cpp tests" ${PROJECT_IS_TOP_LEVEL})
option(LASRS_BUILD_EXAMPLES "Build lasrs-cpp examples" ${PROJECT_IS_TOP_LEVEL})

include(FetchContent)
FetchContent_Declare(Corrosion
  GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
  GIT_TAG v0.6.1
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Corrosion)

corrosion_import_crate(
  MANIFEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/rust/Cargo.toml
  CRATE_TYPES staticlib)

add_library(lasrs INTERFACE)
add_library(lasrs::lasrs ALIAS lasrs)
target_include_directories(lasrs INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_features(lasrs INTERFACE cxx_std_20)
target_link_libraries(lasrs INTERFACE lasrs_ffi)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(DIRECTORY include/lasrs DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES LICENSE-MIT LICENSE-APACHE DESTINATION ${CMAKE_INSTALL_DATADIR}/lasrs)
corrosion_install(TARGETS lasrs_ffi EXPORT lasrs-targets)
install(TARGETS lasrs EXPORT lasrs-targets)

set(config_dir ${CMAKE_INSTALL_LIBDIR}/cmake/lasrs)
install(EXPORT lasrs-targets NAMESPACE lasrs:: DESTINATION ${config_dir})
configure_package_config_file(cmake/lasrs-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/lasrs-config.cmake
  INSTALL_DESTINATION ${config_dir}
  NO_SET_AND_CHECK_MACRO
  NO_CHECK_REQUIRED_COMPONENTS_MACRO)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/lasrs-config-version.cmake
  COMPATIBILITY SameMinorVersion)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/lasrs-config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/lasrs-config-version.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/corrosion/lasrs-targetsCorrosion.cmake
  DESTINATION ${config_dir})

if(LASRS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(LASRS_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
