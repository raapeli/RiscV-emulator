set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-ggdb3)

add_executable(emulator_test test_emulator.cpp)
target_link_libraries(emulator_test PRIVATE emulator)

add_test(NAME EmulatorTests COMMAND emulator_test)
