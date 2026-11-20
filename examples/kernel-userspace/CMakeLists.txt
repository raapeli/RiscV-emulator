find_program(RISCV_GCC riscv64-elf-gcc)
find_program(RISCV_OBJCOPY riscv64-elf-objcopy)
if(NOT RISCV_GCC OR NOT RISCV_OBJCOPY)
  return()
endif()

set(kernel_elf "${CMAKE_CURRENT_BINARY_DIR}/kernel-userspace.elf")
set(kernel_bin "${CMAKE_CURRENT_BINARY_DIR}/kernel-userspace.bin")
add_custom_command(
  OUTPUT "${kernel_bin}"
  BYPRODUCTS "${kernel_elf}"
  COMMAND "${RISCV_GCC}" -march=rv64ima_zicsr -mabi=lp64 -Os -ffreestanding
          -nostdlib -msmall-data-limit=0 -mno-relax -Tlink.ld
          start.S kernel.c -o "${kernel_elf}"
  COMMAND "${RISCV_OBJCOPY}" -O binary "${kernel_elf}" "${kernel_bin}"
  DEPENDS start.S kernel.c link.ld syscalls.h
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  VERBATIM
)
add_custom_target(kernel_userspace ALL DEPENDS "${kernel_bin}")

find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME kernel_user_mode_syscalls_test
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_user_mode_syscalls.py"
            $<TARGET_FILE:RiscV> "${kernel_bin}"
  )
endif()
