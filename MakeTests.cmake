function (build_asm asm_path out_path)
  file(MAKE_DIRECTORY "${out_path}/bin")
  file(MAKE_DIRECTORY "${out_path}/dumped")

  file(GLOB files ${asm_path})
  foreach(file ${files})
    get_filename_component(filename ${file} NAME_WE)
    set(filename_bin "${filename}.bin")
    set(filename_dump "${filename}.dump")

    execute_process(COMMAND riscv64-elf-gcc -Ttests/link.ld
    -Itests/riscv-tests/env/p -Itests/riscv-tests/isa/macros/scalar -nostdlib
    -ffreestanding -march=rv64g -mabi=lp64 -nostartfiles -O0 -o temp ${file})

    execute_process(COMMAND riscv64-elf-objcopy
    -O binary temp ${out_path}/bin/${filename_bin})

    execute_process(OUTPUT_FILE ${out_path}/dumped/${filename_dump}
    COMMAND riscv64-elf-objdump
    --disassemble-all temp)


  endforeach()
endfunction()
execute_process(COMMAND rm -rf testbins)
file(MAKE_DIRECTORY testbins)

build_asm("tests/riscv-tests/isa/rv64ui/*.S" "testbins/rv64ui")
build_asm("tests/riscv-tests/isa/rv64um/*.S" "testbins/rv64um")
build_asm("tests/riscv-tests/isa/rv64ua/*.S" "testbins/rv64ua")
build_asm("tests/riscv-tests/isa/rv64uf/*.S" "testbins/rv64uf")
build_asm("tests/riscv-tests/isa/rv64ud/*.S" "testbins/rv64ud")
build_asm("tests/riscv-tests/isa/rv64uc/*.S" "testbins/rv64uc")

build_asm("tests/riscv-tests/isa/rv64mi/*.S" "testbins/rv64mi")
build_asm("tests/riscv-tests/isa/rv64si/*.S" "testbins/rv64si")

file(REMOVE temp)
