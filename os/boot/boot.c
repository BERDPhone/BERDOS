#define LED_PIN_BOARD 25
#define LED_PIN_GREEN 14
#define LED_PIN_BLUE 17
#define LED_PIN_RED 21

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "../kernel/kernel.h"
#include "malloc.h"

void blinker(unsigned int led_pin) {
    while (true) {
        gpio_init(led_pin);
        gpio_set_dir(led_pin, GPIO_OUT);

        gpio_put(led_pin, 1);
        sleep_ms(500);
        gpio_put(led_pin, 0);
        sleep_ms(500);

    os_yield();
  };
  os_exit();
}

void hi(void) {
    printf("HI!\n");
    os_exit();
}

void hello(void) {
    unsigned int pidn = os_spawn(&hi, NULL);
    os_mkdir(":0", "/:)");
    os_create("File", BERDOS_PARTITION_SIZE * 3, "/:)/:0");

    char buffer[12];
    os_write("/:)/:0/File", "Hello World", 12);
    os_read("/:)/:0/File", buffer, 12);
    printf("%s\n", buffer);

    os_rmdir("/:)/:0");

    while (true) {
        printf("HELLO!\n");
        os_yield();
    };
    os_exit();
}

int main(void) {
    stdio_init_all();
    sleep_ms(5000);

    printf("BOOT: Initializing Kernel \n");
    kernel_initizalize();

    printf("BOOT: Creating Processes \n");
    control_block *pid0 = __create_process(&hello, NULL, NULL);
    unsigned int arguments_1[4] = {LED_PIN_GREEN, 0, 0, 0};
    control_block *pid1 = __create_process(&blinker, arguments_1, pid0);
    unsigned int arguments_2[4] = {LED_PIN_BLUE, 0, 0, 0};
    control_block *pid2 = __create_process(&blinker, arguments_2, pid0);
    unsigned int arguments_3[4] = {LED_PIN_RED, 0, 0, 0};
    control_block *pid3 = __create_process(&blinker, arguments_3, pid0);

    printf("BOOT: Starting Kernel \n");
    kernel_start();

    return 0;
}