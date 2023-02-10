cmake_minimum_required(VERSION 3.13)

project(kernel_project
		LANGUAGES C CXX ASM
		VERSION 0.0.0.0)

add_library(kernel STATIC kernel.c kernel.s)

install(TARGETS kernel DESTINATION lib)
install(FILES kernel.h DESTINATION include)

### set_target_properties(kernel PROPERTIES COMPILE_FLAGS "-save-temps")

target_link_libraries(kernel pico_stdlib pico_multicore pico_sync)