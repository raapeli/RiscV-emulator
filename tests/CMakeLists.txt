add_executable(test_no_jumps test_no_jumps.cpp)
add_executable(test_bus_bounds test_bus_bounds.cpp)
add_executable(test_pmp test_pmp.cpp)
add_executable(test_sv39 test_sv39.cpp)
add_executable(test_floating_rounding test_floating_rounding.cpp)
add_executable(test_interrupt_devices test_interrupt_devices.cpp)
add_executable(test_virtio_blk test_virtio_blk.cpp)
target_link_libraries(test_no_jumps PRIVATE emulator)
target_link_libraries(test_bus_bounds PRIVATE emulator)
target_link_libraries(test_pmp PRIVATE emulator)
target_link_libraries(test_sv39 PRIVATE emulator)
target_link_libraries(test_floating_rounding PRIVATE emulator)
target_link_libraries(test_interrupt_devices PRIVATE emulator)
target_link_libraries(test_virtio_blk PRIVATE emulator)

add_test(NAME integer_instruction_test COMMAND test_no_jumps)
add_test(NAME bus_bounds_test COMMAND test_bus_bounds)
add_test(NAME pmp_test COMMAND test_pmp)
add_test(NAME sv39_test COMMAND test_sv39)
add_test(NAME floating_rounding_test COMMAND test_floating_rounding)
add_test(NAME interrupt_devices_test COMMAND test_interrupt_devices)
add_test(NAME virtio_blk_test COMMAND test_virtio_blk)

set(LINUX_BOOT_TEST_IMAGES "" CACHE PATH
    "Directory containing fw_jump.bin, Image and initramfs.cpio for the Linux boot test")

if(RISCV_BARE_METAL_GCC AND RISCV_BARE_METAL_OBJCOPY AND Python3_Interpreter_FOUND)
  set(boot_probe_elf "${CMAKE_CURRENT_BINARY_DIR}/linux_boot_probe.elf")
  set(boot_probe_bin "${CMAKE_CURRENT_BINARY_DIR}/linux_boot_probe.bin")
  add_custom_command(
    OUTPUT "${boot_probe_bin}"
    BYPRODUCTS "${boot_probe_elf}"
    COMMAND "${RISCV_BARE_METAL_GCC}" -march=rv64ima_zicsr -mabi=lp64 -nostdlib -mno-relax
            -Wl,-Ttext=0x80000000 "${CMAKE_CURRENT_SOURCE_DIR}/linux_boot_probe.S"
            -o "${boot_probe_elf}"
    COMMAND "${RISCV_BARE_METAL_OBJCOPY}" -O binary "${boot_probe_elf}" "${boot_probe_bin}"
    DEPENDS linux_boot_probe.S
    VERBATIM
  )
  add_custom_target(linux_boot_probe ALL DEPENDS "${boot_probe_bin}")
  add_test(NAME linux_boot_loader_test
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_linux_boot_loader.py"
            $<TARGET_FILE:RiscV> "${boot_probe_bin}"
  )
else()
  message(STATUS "Skipping guest boot and syscall tests. Install Python 3 and ${RISCV_TOOLCHAIN_PREFIX}gcc and objcopy")
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

add_executable(test_cpu test_riscv.cpp)
target_link_libraries(test_cpu PRIVATE emulator)
include(IsaTests.cmake)
