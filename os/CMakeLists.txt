# add_subdirectory(drivers/)
add_subdirectory(kernel/)
add_subdirectory(shell/)

add_executable(boot boot/boot.c boot/boot.h) 

### set_target_properties(boot PROPERTIES COMPILE_FLAGS "-save-temps")

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(boot pico_stdlib pico_malloc)
target_link_libraries(boot kernel)
target_link_libraries(boot shell)

# enable usb output, disable uart output
pico_enable_stdio_usb(boot 1)
pico_enable_stdio_uart(boot 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(boot)