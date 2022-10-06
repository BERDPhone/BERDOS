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
  sleep_ms(1000);
  gpio_put(LED_PIN2, 0);
  sleep_ms(1000);
}

void blinker(void) {
  while (true) {
    blink1();
  }
}

int main(void) {

  //blink2();

  kernel_initizalize();

  //blink2();

  create_process(&blinker);

  blink2();

  kernel_start();

  blinker();

  return 0;
}