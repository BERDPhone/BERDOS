cmake_minimum_required(VERSION 3.13)

project(TFT_project)
add_library(TFT STATIC TFT.c)
install(TARGETS TFT DESTINATION lib)
install(FILES headers/TFT.h DESTINATION include)

target_link_libraries(TFT pico_stdlib)