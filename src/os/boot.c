/*
 * Copyright (C) 2022 Keith Standiford
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/stdlib.h"
#include <stdio.h>
#include "pico/malloc.h"
#include "pico/multicore.h"
#include "kernel/lock_core.h"

#include "kernel/kernel.h"

#include <hagl_hal.h>
#include <hagl.h>
#include <stdlib.h>

hagl_backend_t *display;

void continuousDraw(void) {
    while (1) {
        sleep_ms(2000);
        
        color_t color = rand() % 0xffff;
    
        hagl_fill_rectangle(display, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, color);
        hagl_flush(display);
    }
}

bool isPrime(unsigned int number) {
    for (unsigned int i = 3; i <= number / i; i += 2) {
        if (number % i == 0) return true;
    }

    return false;
}

void calcPrimes(void) {
    unsigned int i = 3;
    while (1) {
        if (isPrime(i)) printf("%d\n", i);
        i += 2;
    }
}

int main() {

    set_sys_clock_khz(133000, true);

    stdio_init_all();
    piccolo_init();
    
    display = hagl_init();

    hagl_set_clip_window(display, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    hagl_clear(display);

    piccolo_create_task(continuousDraw);
    piccolo_create_task(calcPrimes);

    piccolo_start();
}

