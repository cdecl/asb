cmake_minimum_required(VERSION 3.5)

project(asb)

set(CMAKE_CXX_FLAGS "-Wall -std=c++14 -O2 -I/usr/local/opt/openssl/include")
link_directories(/usr/local/opt/openssl/lib)
link_libraries(-pthread -lboost_system -lboost_regex -lssl -lcrypto )

file(GLOB src "src/*.cpp")
#set(src xxx.cpp)

add_executable(asb ${src})

