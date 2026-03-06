cmake_minimum_required(VERSION 3.20)

project(core VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

option(CORE_BUILD_CLI "Build the core CLI executable" ON)

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
)

install(
  DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
  EXPORT coreTargets
  NAMESPACE core::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/core
)
