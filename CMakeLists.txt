cmake_minimum_required(VERSION 3.0)
project(FoolFLACenc CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -pedantic")

add_subdirectory(flacutil)

add_executable(dump_flac dump_flac.cpp utility.cpp)
target_link_libraries(dump_flac flacutil)

add_executable(decode_flac decode_flac.cpp utility.cpp)
target_link_libraries(decode_flac flacutil)

add_executable(encode_flac encode_flac.cpp utility.cpp)
target_link_libraries(encode_flac flacutil)

set_property(TARGET dump_flac decode_flac encode_flac PROPERTY CXX_STANDARD 14)
set_property(TARGET dump_flac decode_flac encode_flac PROPERTY CXX_STANDARD_REQUIRED ON)
