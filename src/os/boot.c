/*
Establish communcations with all devices/slaves, test whether connections work, and displays the status 
of the device depending on what devices are operational (via built in LED)

Solid green indicates all good
Blinking green indicates a device is encountering issues

In order to determine which devices are malfunctioning, the LED should blink like so...
With a longer pause between the series of blinks.

short short 				– screen boot issue
short long 					– WiFi boot issue
short short short 			– Microphone boot issue
*/

#include <stdio.h>
#include "pico/stdlib.h"
#include "kernel/kernel.h"
#include "malloc.h"
#include "boot.h"

typedef struct {
	char	*driver_name;
	bool	is_operating;
} __device_driver_status;

__device_driver_status *__driver_initalize();
void __short_led(uint __led_pin);
void __long_led(uint __led_pin);
void signal_driver_status();

int main() {
	
	stdio_init_all();

	__device_driver_status *__device_drivers = __driver_initalize();

	// kernel_initalize();
	
	return 0;
}

void signal_driver_status() {
	const uint __led_pin = PICO_DEFAULT_LED_PIN;
	gpio_init(__led_pin);
	gpio_set_dir(__led_pin, GPIO_OUT);

	int i;
	int len = sizeof(__device_driver_status)/sizeof(__device_drivers);

	for (i = 0; i <= len; i++) {
		if (__device_drivers[i].driver_name == "ILI9341_init.c" && __device_drivers[i].is_operating == false) {
			gpio_put(__led_pin, 0);
			sleep_ms(2000);
			__short_led(__led_pin);
			__short_led(__led_pin);
		} else if (__device_drivers[i].driver_name == "ESP-12E_init.c" && __device_drivers[i].is_operating == false) {
			gpio_put(__led_pin, 0);
			sleep_ms(2000);
			__short_led(__led_pin);
			__long_led(__led_pin);

		} else if (__device_drivers[i].driver_name == "MIC_init.c" && __device_drivers[i].is_operating == false) {
			gpio_put(__led_pin, 0);
			sleep_ms(2000);
			__short_led(__led_pin);
			__short_led(__led_pin);
			__short_led(__led_pin);
		} else {
			gpio_put(__led_pin, 1);
		}
	}
}


void __short_led(uint __led_pin) {
	gpio_put(__led_pin, 1);
	sleep_ms(300);
	gpio_put(__led_pin, 0);
	sleep_ms(100);
}

void __long_led(uint __led_pin) {
	gpio_put(__led_pin, 1);
	sleep_ms(600);
	gpio_put(__led_pin, 0);
	sleep_ms(100);
}


__device_driver_status *__driver_initalize() {
	const int __number_of_drivers = 3;

	__device_driver_status *__device_drivers = malloc(sizeof(__device_driver_status) * __number_of_drivers);

	__device_drivers[0].driver_name = "ILI9341_init.c";
	__device_drivers[0].is_operating = true;

	__device_drivers[1].driver_name = "ESP-12E_init.c";
	__device_drivers[1].is_operating = false;

	__device_drivers[2].driver_name = "MIC_init.c";
	__device_drivers[2].is_operating = true;

	return __device_drivers;
}

