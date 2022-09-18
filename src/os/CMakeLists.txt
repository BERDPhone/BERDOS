add_subdirectory(api/)
add_subdirectory(drivers/)
add_subdirectory(helpers/)
add_subdirectory(kernel/)

add_executable(boot
	boot.c 
	kernel/context_switch.s 
	kernel/kernel.c 
	kernel/kernel.h 
	kernel/lock_core.c 
	kernel/lock_core.h
)

pico_set_program_name(boot "boot")
pico_set_program_version(boot "0.0.1")

# enable usb output, disable uart output
pico_enable_stdio_uart(boot 1)
pico_enable_stdio_usb(boot 0)

target_link_libraries(boot 
	pico_stdlib
	pico_malloc 
	hardware_exception 
	hardware_sync
	pico_multicore
	api
)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(boot)