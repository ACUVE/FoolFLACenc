cmake_minimum_required(VERSION 3.0)

add_library(flacutil STATIC flac_struct_read.cpp flac_struct_write.cpp flac_struct_print.cpp flac_decode.cpp flac_encode.cpp hash.cpp file.cpp)
set_property(TARGET flacutil PROPERTY CXX_STANDARD 14)
set_property(TARGET flacutil PROPERTY CXX_STANDARD_REQUIRED ON)
