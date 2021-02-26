cmake_minimum_required(VERSION 3.5)

project(asb)

add_compile_options( -Wall -std=c++14 -O2 -DFMT_HEADER_ONLY )
link_directories()
link_libraries( pthread boost_system boost_regex ssl crypto )

file(GLOB src "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_executable(asb ${src})

