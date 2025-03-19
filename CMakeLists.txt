cmake_minimum_required(VERSION 3.16)
project(frecker)

set(CMAKE_CXX_STANDARD 20)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
if (COMPILER_SUPPORTS_MARCH_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -march=native -Wall")
endif ()
#
add_library(frecker SHARED
        main.cpp
        board.h
        param.h
        engine.h
        colors.h)

# https://gist.github.com/jlgerber/eafc4ee2b9954e27dd2bb009496b1b03
#add_executable(frecker main.cpp
#        board.h
#        param.h
#        engine.h)
##target_link_libraries(frecker-bin frecker)
# use RUNTIME_OUTPUT_NAME on MSVC
#set_target_properties(frecker-bin PROPERTIES OUTPUT_NAME frecker)


