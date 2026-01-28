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

add_library(frecker SHARED main.cpp)


add_executable(frecker_app main.cpp
        board.h
        param.h
        engine.h
        colors.h
        nnue.h
        endgame.h
        aligned.h
)
target_compile_definitions(frecker_app PRIVATE FRECKER_APP)

target_link_libraries(frecker_app PRIVATE libcmaes::cmaes)


