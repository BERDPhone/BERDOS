#define LED_PIN1 25
#define LED_PIN2 14

#include <stdio.h>
#include "pico/stdlib.h"
#include "kernel/kernel.h"


void blink1(void) {
  gpio_init(LED_PIN1);
  gpio_set_dir(LED_PIN1, GPIO_OUT);

  gpio_put(LED_PIN1, 1);
  sleep_ms(250);
  gpio_put(LED_PIN1, 0);
  sleep_ms(250);
}

void blink2(void) {
  gpio_init(LED_PIN2);
  gpio_set_dir(LED_PIN2, GPIO_OUT);

  gpio_put(LED_PIN2, 1);
  sleep_ms(250);
  gpio_put(LED_PIN2, 0);
  sleep_ms(250);
}

void blinker1(void) {
  while (true) {
    blink1();
    printf("BLINK1 \n");
    piccolo_yield();

  }
}

void blinker2(void) {
  while (true) {
    blink2();
    printf("BLINK2 \n");
    piccolo_yield();
  }
}

int main(void) {
  stdio_init_all();

  blink2();

  printf("***** \n");
  printf("BOOT: Initializing Kernel \n");
  kernel_initizalize();

  printf("BOOT: Creating Processes \n");
  create_process(&blinker1);
  create_process(&blinker2);

  printf("BOOT: Starting Kernel \n");
  kernel_start();

  return 0;
}