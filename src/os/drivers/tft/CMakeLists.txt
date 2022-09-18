add_library(tft_driver STATIC
    pio_tft.c
    pio_tft.h
    pio_tft.pio
    pio_tft.pio.h
)

target_link_libraries(tft_driver 
    pico_stdlib
    hardware_pio
)