cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" CORE_VERSION_TEXT)
string(STRIP "${CORE_VERSION_TEXT}" CORE_VERSION_TEXT)

if(NOT CORE_VERSION_TEXT MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
  message(FATAL_ERROR "VERSION must contain a SemVer version in X.Y.Z format")
endif()

project(core VERSION "${CORE_VERSION_TEXT}" LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

option(CORE_BUILD_CLI "Build the core CLI executable" ON)

set(CORE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/core")

add_library(core INTERFACE)
add_library(core::core ALIAS core)

target_compile_features(core INTERFACE cxx_std_17)
target_include_directories(
  core
  INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(CORE_BUILD_CLI)
  add_executable(core-cli apps/core-cli/main.cpp)
  target_link_libraries(core-cli PRIVATE core::core)
  set_target_properties(core-cli PROPERTIES OUTPUT_NAME core)

  install(
    TARGETS core-cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()

install(
  TARGETS core
  EXPORT coreTargets
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
  DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/coreConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/coreConfig.cmake"
  INSTALL_DESTINATION "${CORE_INSTALL_CMAKEDIR}"
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/coreConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
  ARCH_INDEPENDENT
)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/coreConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/coreConfigVersion.cmake"
  DESTINATION "${CORE_INSTALL_CMAKEDIR}"
)

install(
  EXPORT coreTargets
  FILE coreTargets.cmake
  NAMESPACE core::
  DESTINATION "${CORE_INSTALL_CMAKEDIR}"
)

if(BUILD_TESTING)
  add_executable(core-smoke-test tests/native_smoke.cpp)
  target_link_libraries(core-smoke-test PRIVATE core::core)

  add_test(NAME core-smoke-test COMMAND core-smoke-test)
endif()
