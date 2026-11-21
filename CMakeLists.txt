cmake_minimum_required(VERSION 4.0)
project(RiscV
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_path(MPFR_INCLUDE_DIR NAMES mpfr.h REQUIRED)
find_library(MPFR_LIBRARY NAMES mpfr REQUIRED)
find_path(LIBFDT_INCLUDE_DIR NAMES libfdt.h REQUIRED)
find_library(LIBFDT_LIBRARY NAMES fdt REQUIRED)

add_compile_options(-Wall -Wextra -Wpedantic
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:-fcolor-diagnostics>
    $<$<CONFIG:RelWithDebInfo>:-fno-omit-frame-pointer>)
add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)
add_executable(RiscV
  src/main.cpp
  src/linux_boot.cpp
)

add_library(emulator src/cpu.cpp src/bus.cpp src/csrs.cpp src/exception.cpp
            src/interrupt.cpp src/instructions.cpp src/sv39.cpp src/compressed.cpp
            src/floating.cpp src/devices/clint.cpp src/devices/plic.cpp
            src/devices/uart.cpp src/devices/virtio_blk.cpp)
target_include_directories(emulator PUBLIC include)
target_include_directories(emulator PRIVATE ${MPFR_INCLUDE_DIR})
target_link_libraries(emulator PRIVATE ${MPFR_LIBRARY})

target_include_directories(RiscV PRIVATE ${LIBFDT_INCLUDE_DIR})
target_link_libraries(RiscV PRIVATE emulator ${LIBFDT_LIBRARY})

set(RISCV_TOOLCHAIN_PREFIX "riscv64-elf-" CACHE STRING
    "Prefix for the bare-metal RISC-V GCC and binutils")
find_program(RISCV_BARE_METAL_GCC NAMES "${RISCV_TOOLCHAIN_PREFIX}gcc" NO_CACHE)
find_program(RISCV_BARE_METAL_OBJCOPY NAMES "${RISCV_TOOLCHAIN_PREFIX}objcopy" NO_CACHE)

include(CTest)
if(BUILD_TESTING)
  find_package(Python3 QUIET COMPONENTS Interpreter)
  add_subdirectory(tests)
endif()
add_subdirectory(examples/kernel-userspace)
