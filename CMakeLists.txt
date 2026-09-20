cmake_minimum_required(VERSION 3.20)
project(hypr-agent-cursors LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(GTest REQUIRED)

add_library(hypr_agent_cursors_core STATIC
  src/agent_seats.cpp
  src/live_session_guard.cpp
  src/workspace_lease.cpp
)
target_include_directories(hypr_agent_cursors_core PUBLIC include)

add_executable(hypr_agent_cursors_tests
  tests/test_agent_seats.cpp
  tests/test_live_session_guard.cpp
  tests/test_workspace_lease.cpp
)
target_link_libraries(hypr_agent_cursors_tests
  PRIVATE
    hypr_agent_cursors_core
    GTest::gtest_main
)

enable_testing()
add_test(NAME hypr_agent_cursors_tests COMMAND hypr_agent_cursors_tests)

# Plugin .so is opt-in. Never load it into the live human Hyprland session.
option(BUILD_HYPR_PLUGIN "Build the Hyprland plugin .so (load only in a nested instance)" OFF)
if(BUILD_HYPR_PLUGIN)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(HYPRLAND REQUIRED IMPORTED_TARGET hyprland)
  add_library(hypr-agent-cursors MODULE src/plugin.cpp)
  target_link_libraries(hypr-agent-cursors PRIVATE hypr_agent_cursors_core PkgConfig::HYPRLAND)
  target_compile_options(hypr-agent-cursors PRIVATE -fPIC --no-gnu-unique)
  set_target_properties(hypr-agent-cursors PROPERTIES PREFIX "" OUTPUT_NAME "hypr-agent-cursors")
endif()
