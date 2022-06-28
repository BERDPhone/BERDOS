/*
Establish communcations with all devices/slaves, test whether connections work, and displays the status of the device depending on what devices are operational (via built in LED)

Solid green indicates all good
Blinking green indicates a device is encountering issues

In order to determine which devices are malfunctioning, the LED should blink like so...
With a longer pause between the series of blinks.

short short 				– screen boot issue
short long 					– WiFi boot issue
short short short 			– Cellular boot issue
*/

#include <stdio.h>
#include "pico/stdlib.h"

int main() {
    
    stdio_init_all();
    while (1) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }

    
    return 0;
}
