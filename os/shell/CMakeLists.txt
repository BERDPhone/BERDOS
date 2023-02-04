cmake_minimum_required(VERSION 3.13)

project(shell_project
		LANGUAGES C CXX ASM
		VERSION 0.0.0.0)

add_library(shell STATIC shell.c)

install(TARGETS shell DESTINATION lib)
install(FILES shell.h DESTINATION include)

### set_target_properties(shell PROPERTIES COMPILE_FLAGS "-save-temps")

target_link_libraries(shell pico_stdlib)
target_link_libraries(shell kernel)