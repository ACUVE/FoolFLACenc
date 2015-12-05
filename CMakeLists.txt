cmake_minimum_required(VERSION 3.0)
project(FoolFLACenc CXX)

add_executable(dump_flac dump_flac.cpp flac_struct_read.cpp flac_struct_print.cpp hash.cpp)
set_property(TARGET dump_flac PROPERTY CXX_STANDARD 14)
set_property(TARGET dump_flac PROPERTY CXX_STANDARD_REQUIRED ON)

add_executable(decode_flac decode_flac.cpp flac_struct_read.cpp flac_struct_print.cpp flac_decode.cpp hash.cpp)
set_property(TARGET decode_flac PROPERTY CXX_STANDARD 14)
set_property(TARGET decode_flac PROPERTY CXX_STANDARD_REQUIRED ON)
