cmake_minimum_required(VERSION 2.6)
project(FoolFLACenc)

add_executable(dump_flac dump_flac.cpp flac_struct_read.cpp flac_struct_print.cpp hash.cpp)
set_property(TARGET dump_flac PROPERTY CXX_STANDARD 14)
set_property(TARGET dump_flac PROPERTY CXX_STANDARD_REQUIRED ON)
