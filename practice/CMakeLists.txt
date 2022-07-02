project(arm_practice C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_SOURCE_DIR})

# include_directories(${CMAKE_SOURCE_DIR})

add_executable(arm_practice
    arm_practice.s
)

pico_enable_stdio_uart(arm_practice 0)
pico_enable_stdio_usb(arm_practice 1)

pico_add_extra_outputs(arm_practice)

target_link_libraries(arm_practice pico_stdlib)
pico_add_extra_outputs(arm_practice)