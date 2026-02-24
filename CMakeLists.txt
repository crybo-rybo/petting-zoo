cmake_minimum_required(VERSION 3.24)
project(petting_zoo VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PETTING_ZOO_BUILD_WEB "Build web assets with npm when available" ON)
option(PETTING_ZOO_WEB_ALLOW_FALLBACK "Allow fallback static web assets when npm web build fails" OFF)
option(PETTING_ZOO_ENABLE_MCP "Enable MCP support via zoo-keeper" ON)
option(PETTING_ZOO_BUILD_TESTS "Build and register project tests" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FetchDependencies)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/zoo-keeper/CMakeLists.txt")
  message(FATAL_ERROR "zoo-keeper submodule is missing. Run: git submodule update --init --recursive")
endif()

set(ZOO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZOO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZOO_ENABLE_MCP ${PETTING_ZOO_ENABLE_MCP} CACHE BOOL "" FORCE)
add_subdirectory(zoo-keeper EXCLUDE_FROM_ALL)

add_subdirectory(apps/web)
add_subdirectory(apps/server)

if(PETTING_ZOO_BUILD_TESTS)
  set(BUILD_TESTING ON CACHE BOOL "Enable CTest tests" FORCE)
  include(CTest)
  add_subdirectory(tests)
endif()
