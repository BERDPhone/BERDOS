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

// int main() {
	
// 	stdio_init_all();
// 	while (1) {
// 		printf("Hello, world!\n");

// 		const uint LED_PIN = PICO_DEFAULT_LED_PIN;
// 		gpio_init(LED_PIN);
// 		gpio_set_dir(LED_PIN, GPIO_OUT);

// 		gpio_put(LED_PIN, 1);
// 		sleep_ms(250);
// 		gpio_put(LED_PIN, 0);
// 		sleep_ms(250);
// 	}

	
// 	return 0;
// }


// const uint LED_PIN = 25;
// const uint LED2_PIN = 14;

// void task1_func(void) {
// 	piccolo_sleep_t t;
// 	gpio_init(LED_PIN);
// 	gpio_set_dir(LED_PIN, GPIO_OUT);
// 	while (true) {
// 		gpio_put(LED_PIN, 1);
// 		piccolo_sleep(&t, 1000);
// 		gpio_put(LED_PIN, 0);
// 		piccolo_sleep(&t, 1000);
// 	}
// }

// int is_prime(unsigned int n)
// {
// 	unsigned int p;
// 	if (!(n & 1) || n < 2 ) return n == 2;
 
// 	/* comparing p*p <= n can overflow */
// 	for (p = 3; p <= n/p; p += 2)
// 		if (!(n % p)) return 0;
// 	return 1;
// }

// void task2_func(void) {
// 	piccolo_sleep_t t;
// 	int p;

// 	printf("task2: Created!\n");
// 	while (1) {
// 		p = to_ms_since_boot(get_absolute_time());
// 		if(is_prime(p)==1) {
// 			printf("%d is prime!\n", p);
// 		}
// 		piccolo_yield();
// 	}
// }

// void task3_func(void) {
// 	piccolo_sleep_t t;
// 	gpio_init(LED2_PIN);
// 	gpio_set_dir(LED2_PIN, GPIO_OUT);
// 	while (true) {
// 		gpio_put(LED2_PIN, 1);
// 		piccolo_sleep(&t, 75);
// 		gpio_put(LED2_PIN, 0);
// 		piccolo_sleep(&t, 75);
// 	}
// }

int main() {
	piccolo_init();
	stdio_init_all();

	while (1) {
		// printf("PICCOLO OS Demo Starting...\n");
	}

	// piccolo_create_task(&task1_func);
	// piccolo_create_task(&task2_func);
	// piccolo_create_task(&task3_func);

	// piccolo_start();

	return 0; /* Never gonna happen */
}