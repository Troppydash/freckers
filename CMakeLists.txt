cmake_minimum_required(VERSION 3.16)
project(frecker)

set(CMAKE_CXX_STANDARD 20)

find_package(libcmaes REQUIRED)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
if (COMPILER_SUPPORTS_MARCH_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -Wall -fopt-info-vec-optimized=vec.log")
    # -ftrapv #  -fopenmp-simd
endif ()

# lib
add_library(frecker SHARED main.cpp)
target_compile_definitions(frecker PRIVATE FRECKER_HARDCODE_CONFIG)

# tuner
add_executable(frecker_tuner main.cpp)
target_compile_definitions(frecker_tuner PRIVATE FRECKER_TUNER)
target_link_libraries(frecker_tuner PRIVATE libcmaes::cmaes)


add_executable(frecker_test main.cpp)
target_compile_definitions(frecker_test PRIVATE FRECKER_HARDCODE_CONFIG)


