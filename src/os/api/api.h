#pragma once

#include <stdint.h>

void begin();

void fill_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
               uint16_t color);

// For testing.
bool is_overrun();
