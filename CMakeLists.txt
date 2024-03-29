cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(NAME BDOS)

# initialize pico-sdk from GIT
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_FETCH_FROM_GIT on)

# pico_sdk_import.cmake is a single file copied from this SDK
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(${NAME}
	LANGUAGES C CXX ASM)

list(APPEND PICO_CONFIG_HEADER_FILES ${PROJECT_SOURCE_DIR}/src/os/kernel/lock_core.h)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project

# Add hello world example
add_subdirectory(src)

add_compile_options(-Wall
-Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
-Wno-unused-function # we have some for the docs that aren't called
-Wno-maybe-uninitialized
)
