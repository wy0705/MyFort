cmake_minimum_required(VERSION 3.20)
project(wy_cuda_test)

set(CMAKE_CXX_STANDARD 14)

add_executable(wy_cuda_test main.cpp master.h Lump.h)
