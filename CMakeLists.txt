cmake_minimum_required(VERSION 4.0)
project(RiscV
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-ggdb3 -Wall -Wextra -Wpedantic -fcolor-diagnostics -DDEBUG)
add_executable(RiscV
  src/main.cpp
  src/lexer.cpp
  src/parser.cpp
  src/bus.cpp
  src/cpu.cpp
  src/exception.cpp
  src/csrs.cpp
)

add_library(emulator src/cpu.cpp src/bus.cpp src/csrs.cpp)
target_include_directories(emulator PUBLIC include)

target_include_directories(RiscV
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

enable_testing()
add_subdirectory(tests)
