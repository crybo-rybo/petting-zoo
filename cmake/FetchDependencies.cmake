include(FetchContent)

find_package(Drogon QUIET)
if(NOT Drogon_FOUND)
  message(STATUS "Drogon not found, fetching from source")
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_CTL OFF CACHE BOOL "" FORCE)
  set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(BUILD_ORM OFF CACHE BOOL "" FORCE)
  set(BUILD_REDIS OFF CACHE BOOL "" FORCE)
  set(BUILD_MYSQL OFF CACHE BOOL "" FORCE)
  set(BUILD_POSTGRESQL OFF CACHE BOOL "" FORCE)
  set(BUILD_SQLITE ON CACHE BOOL "" FORCE)
  FetchContent_Declare(
    drogon
    GIT_REPOSITORY https://github.com/drogonframework/drogon.git
    GIT_TAG v1.9.8
  )
  FetchContent_MakeAvailable(drogon)
endif()
