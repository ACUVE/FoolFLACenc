cmake_minimum_required(VERSION 2.6)
project(FoolFLACenc)

add_executable(ffe main.cpp flac_struct_read.cpp hash.cpp)
set_property(TARGET ffe PROPERTY CXX_STANDARD 14)
set_property(TARGET ffe PROPERTY CXX_STANDARD_REQUIRED ON)
