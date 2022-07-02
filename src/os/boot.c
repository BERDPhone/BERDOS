/*
Establish communcations with all devices/slaves, test whether connections work, and displays the status 
of the device depending on what devices are operational (via built in LED)

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
#include "kernel/kernel.h"
#include "malloc.h"





// // just use link list for this data structure
// __device_driver_status* __driver_initalize() {
// 	// const int __number_of_drivers = 1;
// 	*__device_driver_status __device_driver_array[1] = malloc(sizeof(__device_driver_status));

// 	__device_driver_status[0].driver_name = "ILI9341.h";
// 	__device_driver_status[0].is_operating = true;

// 	return __device_driver_array;
// }

typedef struct {
	char	*driver_name;
	bool	is_operating;
} __device_driver_status;


// __device_driver_status *fun(int k);


__device_driver_status *__driver_initalize() {
	const int __number_of_drivers = 3;

	__device_driver_status *__device_drivers = malloc(sizeof(__device_driver_status) * __number_of_drivers);

	__device_drivers[0].driver_name = "ILI9341_init.c";
	__device_drivers[0].is_operating = true;

	__device_drivers[1].driver_name = "ESP-12E_init.c";
	__device_drivers[1].is_operating = true;

	__device_drivers[2].driver_name = "MIC_init.c";
	__device_drivers[2].is_operating = true;

	return __device_drivers;
}

int main() {
	
	stdio_init_all();

	__device_driver_status *__device_drivers = __driver_initalize();

	

	// kernel_initalize();



	while (1) {
		int i;
		int len = sizeof(__device_driver_status)/sizeof(__device_drivers);

		for (i = 0; i <= len; i++) {
			printf("\n\n=======================\n");
			printf("i: %i\n", i);
			printf("len: %i\n", len);
			printf("driver_name: %s, \n", __device_drivers[i].driver_name);
			printf("is_operating: %s\n", __device_drivers[i].is_operating ? "true" : "false");
			printf("=======================\n\n");
		}

		sleep_ms(1000);

		const uint LED_PIN = PICO_DEFAULT_LED_PIN;
		gpio_init(LED_PIN);
		gpio_set_dir(LED_PIN, GPIO_OUT);

		gpio_put(LED_PIN, 1);
	}

	
	return 0;
}