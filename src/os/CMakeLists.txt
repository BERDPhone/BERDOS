add_subdirectory(api/)
add_subdirectory(drivers/)
add_subdirectory(helpers/)
add_subdirectory(kernal/)

add_executable(boot
    boot.c
)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(boot pico_stdlib)
target_link_libraries(boot kernal)

# enable usb output, disable uart output
pico_enable_stdio_usb(boot 1)
pico_enable_stdio_uart(boot 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(boot)