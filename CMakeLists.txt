cmake_minimum_required(VERSION 3.5)

project(asb)

add_compile_options( -Wall -std=c++14 -O2 )
link_directories()
#link_libraries( -pthread boost_system boost_regex ssl crypto z dl -static -static-libstdc++ -no-pie )
link_libraries( pthread boost_system boost_regex ssl crypto )

file(GLOB src "${CMAKE_SOURCE_DIR}/src/*.cpp")
# SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

add_executable(asb ${src})

