cmake_minimum_required(VERSION 3.13)

project(kernal_project)
add_library(kernal STATIC kernal.c)
install(TARGETS kernal DESTINATION lib)
install(FILES kernal.h DESTINATION include)

target_link_libraries(kernal pico_stdlib)