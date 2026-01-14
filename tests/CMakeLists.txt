set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-ggdb3 -Wall -Wextra -Wpedantic)

add_executable(test_no_jumps test_no_jumps.cpp)
add_executable(test_jumps test_jumps.cpp)
add_executable(test_cpu test_riscv.cpp)
target_link_libraries(test_no_jumps PRIVATE emulator)
target_link_libraries(test_jumps PRIVATE emulator)
target_link_libraries(test_cpu PRIVATE emulator)

add_test(NAME no_jumps COMMAND test_no_jumps)
add_test(NAME jumps COMMAND test_jumps)
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
