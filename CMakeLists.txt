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
  target_compile_options(asb PRIVATE /DFMT_HEADER_ONLY /MT)
else()
  target_compile_options(asb PRIVATE -DFMT_HEADER_ONLY -O2)
endif()

find_package(OpenSSL REQUIRED)
target_link_libraries(asb PRIVATE OpenSSL::SSL OpenSSL::Crypto)

find_package(Boost REQUIRED COMPONENTS regex)
target_link_libraries(asb PRIVATE Boost::regex)
