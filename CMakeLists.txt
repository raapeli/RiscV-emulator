cmake_minimum_required(VERSION 4.0)
project(RiscV)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-ggdb3)
add_executable(RiscV main.cpp instructions.cpp instructions.h)
