cmake_minimum_required(VERSION 4.0)
project(RiscV
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_path(MPFR_INCLUDE_DIR NAMES mpfr.h REQUIRED)
find_library(MPFR_LIBRARY NAMES mpfr REQUIRED)

add_compile_options(-ggdb3 -Wall -Wextra -Wpedantic -fcolor-diagnostics
      -DDEBUG -std=c++23)
add_executable(RiscV
  src/main.cpp
  src/bus.cpp
  src/cpu.cpp
  src/exception.cpp
  src/csrs.cpp
  src/interrupt.cpp
  src/devices/clint.cpp
  src/devices/plic.cpp
  src/sv39.cpp
  src/compressed.cpp
  src/floating.cpp
  src/instructions.cpp
)

add_library(emulator src/cpu.cpp src/bus.cpp src/csrs.cpp src/exception.cpp
            src/interrupt.cpp src/instructions.cpp src/sv39.cpp src/compressed.cpp
            src/floating.cpp src/devices/clint.cpp src/devices/plic.cpp)
target_include_directories(emulator PUBLIC include)

target_include_directories(RiscV
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

foreach(target RiscV emulator)
  target_include_directories(${target} PRIVATE ${MPFR_INCLUDE_DIR})
  target_link_libraries(${target} PRIVATE ${MPFR_LIBRARY})
endforeach()

enable_testing()
add_subdirectory(tests)
add_subdirectory(examples/kernel-userspace)
