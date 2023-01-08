#define LED_PIN_BOARD 25
#define LED_PIN_GREEN 14
#define LED_PIN_BLUE 17
#define LED_PIN_RED 21

#include <stdio.h>
#include "pico/stdlib.h"
#include "../kernel/kernel.h"

void blinker(unsigned int led_pin) {
    while (true) {
        gpio_init(led_pin);
        gpio_set_dir(led_pin, GPIO_OUT);

        gpio_put(led_pin, 1);
        sleep_ms(1000);
        gpio_put(led_pin, 0);
        sleep_ms(1000);

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
  unsigned int pid0 = create_process(&hello, NULL, NULL);
  unsigned int arguments_1[4] = {LED_PIN_GREEN, 0, 0, 0};
  unsigned int pid1 = create_process(&blinker, arguments_1, __get_process_by_id_number(0));
  unsigned int arguments_2[4] = {LED_PIN_BLUE, 0, 0, 0};
  unsigned int pid2 = create_process(&blinker, arguments_2, __get_process_by_id_number(0));
  unsigned int arguments_3[4] = {LED_PIN_RED, 0, 0, 0};
  unsigned int pid3 = create_process(&blinker, arguments_3, __get_process_by_id_number(0));

  printf("BOOT: Starting Kernel \n");
  kernel_start();

  return 0;
}