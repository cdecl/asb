cmake_minimum_required(VERSION 3.11)

project(asb)

# add_compile_options( -Wall -std=c++14 -O2 -DFMT_HEADER_ONLY )
# link_directories()
# link_libraries( pthread boost_system boost_regex ssl crypto )

# set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD 17)

file(GLOB src "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_executable(asb ${src})

if (MSVC) 
  target_compile_options(asb PRIVATE /MT)
  target_compile_definitions(asb PRIVATE FMT_HEADER_ONLY _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(asb PRIVATE -O2)
  target_compile_definitions(asb PRIVATE FMT_HEADER_ONLY)
endif()

find_package(OpenSSL REQUIRED)
target_link_libraries(asb PRIVATE OpenSSL::SSL OpenSSL::Crypto)

find_package(Boost REQUIRED COMPONENTS regex)
target_link_libraries(asb PRIVATE Boost::regex)
