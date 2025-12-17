set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-ggdb3)

add_executable(test_no_jumps test_no_jumps.cpp)
add_executable(test_jumps test_jumps.cpp)
target_link_libraries(test_no_jumps PRIVATE emulator)
target_link_libraries(test_jumps PRIVATE emulator)

add_test(NAME no_jumps COMMAND test_no_jumps)
add_test(NAME jumps COMMAND test_jumps)
