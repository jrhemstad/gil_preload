cmake_minimum_required(VERSION 3.14)

include(cmake/CPM.cmake)


CPMAddPackage(
    NAME NVTX
    GITHUB_REPOSITORY NVIDIA/NVTX
    GIT_TAG dev
    SOURCE_SUBDIR cpp
    OPTIONS
       "BUILD_TESTS OFF"
       "BUILD_BENCHMARKS OFF"
    )
    

add_library(gil_preload SHARED gil_preload.cpp)

target_compile_features(gil_preload PRIVATE cxx_std_14)

target_link_libraries(gil_preload nvtx3-cpp pthread dl)

