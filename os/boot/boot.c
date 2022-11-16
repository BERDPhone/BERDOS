#define LED_PIN1 25
#define LED_PIN2 14
#define LED_PIN3 17
#define LED_PIN4 21

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

    piccolo_yield();
  };
  piccolo_yield();
}

int main(void) {
  stdio_init_all();

  sleep_ms(5000);

  printf("BOOT: Initializing Kernel \n");
  kernel_initizalize();

  printf("BOOT: Creating Processes \n");
  unsigned int pid0 = create_process(&blinker, 1, LED_PIN1);
  unsigned int pid1 = create_process(&blinker, 1, LED_PIN2);
  unsigned int pid2 = create_process(&blinker, 1, LED_PIN3);
  unsigned int pid3 = create_process(&blinker, 1, LED_PIN4);

  terminate_process(pid0);

  printf("BOOT: Starting Kernel \n");
  kernel_start();

  return 0;
}