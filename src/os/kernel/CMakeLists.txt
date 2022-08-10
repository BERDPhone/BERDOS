cmake_minimum_required(VERSION 3.13)

project(kernel_project)
add_library(kernel STATIC kernel.c kernel.s)
# add_library(kernel STATIC kernel.c)
install(TARGETS kernel DESTINATION lib)
install(FILES kernel.h DESTINATION include)

target_link_libraries(kernel pico_stdlib)