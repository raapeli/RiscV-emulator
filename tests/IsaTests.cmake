find_program(RISCV_BARE_METAL_OBJDUMP NAMES "${RISCV_TOOLCHAIN_PREFIX}objdump" NO_CACHE)
if(NOT RISCV_BARE_METAL_GCC OR NOT RISCV_BARE_METAL_OBJCOPY OR NOT RISCV_BARE_METAL_OBJDUMP)
  message(STATUS "Skipping ISA suites. Install ${RISCV_TOOLCHAIN_PREFIX}gcc, objcopy, and objdump")
  return()
endif()

set(isa_root "${CMAKE_CURRENT_SOURCE_DIR}/riscv-tests")
set(isa_linker_script "${CMAKE_CURRENT_SOURCE_DIR}/link.ld")
if(NOT EXISTS "${isa_root}/isa/rv64ui/add.S" OR
   NOT EXISTS "${isa_root}/env/p/riscv_test.h")
  message(STATUS "Skipping ISA suites. Run git submodule update --init --recursive")
  return()
endif()

set(isa_outputs)
foreach(suite rv64ui rv64um rv64ua rv64uf rv64ud rv64uc rv64mi rv64si)
  file(GLOB sources CONFIGURE_DEPENDS "${isa_root}/isa/${suite}/*.S")
  set(suite_dir "${CMAKE_CURRENT_BINARY_DIR}/isa/${suite}")
  set(suite_binaries)
  foreach(source IN LISTS sources)
    get_filename_component(name "${source}" NAME_WE)
    set(elf "${suite_dir}/${name}.elf")
    set(binary "${suite_dir}/bin/${name}.bin")
    set(dump "${suite_dir}/dumped/${name}.dump")
    set(depfile "${suite_dir}/${name}.d")
    add_custom_command(
      OUTPUT "${binary}" "${dump}"
      BYPRODUCTS "${elf}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${suite_dir}/bin" "${suite_dir}/dumped"
      COMMAND "${RISCV_BARE_METAL_GCC}" -T "${isa_linker_script}"
              -I "${isa_root}/env/p" -I "${isa_root}/isa/macros/scalar"
              -nostdlib -ffreestanding -march=rv64g -mabi=lp64 -nostartfiles -O0
              -MMD -MF "${depfile}" -MT "${binary}" -o "${elf}" "${source}"
      COMMAND "${RISCV_BARE_METAL_OBJCOPY}" -O binary "${elf}" "${binary}"
      COMMAND "${RISCV_BARE_METAL_OBJDUMP}" --disassemble-all "${elf}" > "${dump}"
      DEPENDS "${source}" "${isa_linker_script}"
      DEPFILE "${depfile}"
      VERBATIM
    )
    list(APPEND suite_binaries "${binary}")
    list(APPEND isa_outputs "${binary}" "${dump}")
  endforeach()
  add_test(NAME ${suite}_test COMMAND test_cpu ${suite_binaries}
           WORKING_DIRECTORY "${suite_dir}")
endforeach()

add_custom_target(isa_test_images DEPENDS ${isa_outputs})
add_dependencies(test_cpu isa_test_images)
