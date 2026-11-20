set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-ggdb3 -Wall -Wextra -Wpedantic)

add_executable(test_no_jumps test_no_jumps.cpp)
add_executable(test_cpu test_riscv.cpp)
add_executable(test_bus_bounds test_bus_bounds.cpp)
add_executable(test_pmp test_pmp.cpp)
add_executable(test_sv39 test_sv39.cpp)
add_executable(test_floating_rounding test_floating_rounding.cpp)
add_executable(test_interrupt_devices test_interrupt_devices.cpp)
target_link_libraries(test_no_jumps PRIVATE emulator)
target_link_libraries(test_cpu PRIVATE emulator)
target_link_libraries(test_bus_bounds PRIVATE emulator)
target_link_libraries(test_pmp PRIVATE emulator)
target_link_libraries(test_sv39 PRIVATE emulator)
target_link_libraries(test_floating_rounding PRIVATE emulator)
target_link_libraries(test_interrupt_devices PRIVATE emulator)

add_test(NAME integer_instruction_test COMMAND test_no_jumps)
add_test(NAME bus_bounds_test COMMAND test_bus_bounds)
add_test(NAME pmp_test COMMAND test_pmp)
add_test(NAME sv39_test COMMAND test_sv39)
add_test(NAME floating_rounding_test COMMAND test_floating_rounding)
add_test(NAME interrupt_devices_test COMMAND test_interrupt_devices)

set(LINUX_BOOT_TEST_IMAGES "" CACHE PATH
    "Directory containing fw_jump.bin, Image and initramfs.cpio for the Linux boot test")

find_program(RISCV_GCC riscv64-elf-gcc)
find_program(RISCV_OBJCOPY riscv64-elf-objcopy)
find_package(Python3 QUIET COMPONENTS Interpreter)
if(RISCV_GCC AND RISCV_OBJCOPY AND Python3_Interpreter_FOUND)
  set(boot_probe_elf "${CMAKE_CURRENT_BINARY_DIR}/linux_boot_probe.elf")
  set(boot_probe_bin "${CMAKE_CURRENT_BINARY_DIR}/linux_boot_probe.bin")
  add_custom_command(
    OUTPUT "${boot_probe_bin}"
    BYPRODUCTS "${boot_probe_elf}"
    COMMAND "${RISCV_GCC}" -march=rv64ima_zicsr -mabi=lp64 -nostdlib -mno-relax
            -Wl,-Ttext=0x80000000 "${CMAKE_CURRENT_SOURCE_DIR}/linux_boot_probe.S"
            -o "${boot_probe_elf}"
    COMMAND "${RISCV_OBJCOPY}" -O binary "${boot_probe_elf}" "${boot_probe_bin}"
    DEPENDS linux_boot_probe.S
    VERBATIM
  )
  add_custom_target(linux_boot_probe ALL DEPENDS "${boot_probe_bin}")
  add_test(NAME linux_boot_loader_test
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_linux_boot_loader.py"
            $<TARGET_FILE:RiscV> "${boot_probe_bin}"
  )
endif()

if(LINUX_BOOT_TEST_IMAGES)
  if(NOT Python3_Interpreter_FOUND)
    message(FATAL_ERROR "The Linux boot integration test requires Python 3")
  endif()
  foreach(image fw_jump.bin Image initramfs.cpio)
    set(image_path "${LINUX_BOOT_TEST_IMAGES}/${image}")
    if(NOT EXISTS "${image_path}" OR IS_DIRECTORY "${image_path}")
      message(FATAL_ERROR "Missing Linux boot test image: ${image_path}")
    endif()
  endforeach()

  add_test(NAME linux_busybox_boot_test
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_linux_busybox.py"
            $<TARGET_FILE:RiscV> "${LINUX_BOOT_TEST_IMAGES}"
            "${CMAKE_CURRENT_BINARY_DIR}/linux_busybox_boot.log"
  )
  set_tests_properties(linux_busybox_boot_test PROPERTIES TIMEOUT 190)
endif()

add_test(NAME rv64ui_test
 COMMAND $<TARGET_FILE:test_cpu> "../../testbins/rv64ui/bin/"
)

add_test(NAME rv64um_test
 COMMAND $<TARGET_FILE:test_cpu> "../../testbins/rv64um/bin/"
)

add_test(NAME rv64ua_test
  COMMAND $<TARGET_FILE:test_cpu> "../../testbins/rv64ua/bin/"
)

add_test(NAME rv64uf_test
  COMMAND $<TARGET_FILE:test_cpu> "../../testbins/rv64uf/bin/"
)

add_test(NAME rv64ud_test
  COMMAND $<TARGET_FILE:test_cpu> "../../testbins/rv64ud/bin/"
)

add_test(NAME rv64uc_test
  COMMAND $<TARGET_FILE:test_cpu> "../../testbins/rv64uc/bin/"
)

add_test(NAME rv64mi_test
  COMMAND $<TARGET_FILE:test_cpu> "../../testbins/rv64mi/bin/"
)

add_test(NAME rv64si_test
  COMMAND $<TARGET_FILE:test_cpu> "../../testbins/rv64si/bin/"
)
