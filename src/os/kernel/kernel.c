/*
kernel.c manages the proccess and the safty of those proccess
i.e. prevent crashing of entire system from process accessing wrong memory or null.
*/

#include "pico/stdlib.h"
#include <stdio.h>
#include "kernel.h"

void kernel_initalize() {
	stdio_init_all();
	printf("Kernel: Kernal being initaized.\n");
}

void kernel_start() {
	stdio_init_all();
	printf("Kernel: Kernal starting.\n");
	while (1) {
	    // statement
	}
}

void kernel_create_process(void (*pointer_to_task_function)(void)) {
	// stdio_init_all();
	printf("Kernel: Creating a kernel process.\n");

	while (1) {
		pointer_to_task_function();
	}
}

void kernel_kill_process(void (*pointer_to_task_function)(void)) {
	
}