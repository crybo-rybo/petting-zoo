cmake_minimum_required(VERSION 3.24)
project(petting_zoo VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PETTING_ZOO_BUILD_WEB "Build web assets with npm when available" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FetchDependencies)

add_subdirectory(apps/web)
add_subdirectory(apps/server)
