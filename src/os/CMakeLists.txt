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
	hardware_spi
	hardware_dma
	hagl
	hagl_hal
)

target_compile_definitions(boot PRIVATE
	HAGL_HAL_USE_DOUBLE_BUFFER
	HAGL_HAL_USE_DMA
	HAGL_HAL_DEBUG
	MIPI_DISPLAY_PIN_CS=9
    MIPI_DISPLAY_PIN_DC=8
    MIPI_DISPLAY_PIN_RST=12
    MIPI_DISPLAY_PIN_BL=13
    MIPI_DISPLAY_PIN_CLK=10
    MIPI_DISPLAY_PIN_MOSI=11
    MIPI_DISPLAY_PIN_MISO=-1
    MIPI_DISPLAY_SPI_PORT=spi1
#	MIPI_DISPLAY_SPI_CLOCK_SPEED_HZ=680000

#	MIPI_DISPLAY_PIXEL_FORMAT=MIPI_DCS_PIXEL_FORMAT_16BIT
#	MIPI_DISPLAY_ADDRESS_MODE=MIPI_DCS_ADDRESS_MODE_RGB

    MIPI_DISPLAY_WIDTH=320
    MIPI_DISPLAY_HEIGHT=480
    MIPI_DISPLAY_OFFSET_X=0
    MIPI_DISPLAY_OFFSET_Y=0
)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(boot)
