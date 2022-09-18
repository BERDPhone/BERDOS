add_library(api STATIC api.c api.h)

target_link_libraries(api PUBLIC tft_driver)