#include <stdio.h>

#include "pico/stdlib.h"

#define LED_USER1_PIN 25 // user LED 1 on the EP-0172 carrier
#define LED_USER2_PIN 17 // user LED 2 on the EP-0172 carrier

int main(void) {
  stdio_init_all(); // route printf to UART0 (and USB CDC), per CMakeLists

  gpio_init(LED_USER1_PIN);
  gpio_set_dir(LED_USER1_PIN, GPIO_OUT);
  gpio_init(LED_USER2_PIN);
  gpio_set_dir(LED_USER2_PIN, GPIO_OUT);

  printf("l0-blink starting: LED1=GP%d LED2=GP%d\n", LED_USER1_PIN,
         LED_USER2_PIN);

  bool on = false;
  uint32_t tick = 0;
  while (true) {
    on = !on;
    gpio_put(LED_USER1_PIN, on);

    gpio_put(LED_USER2_PIN, !on); // opposite phase: easy to see both work
    printf("tick %lu: LED1=%d LED2=%d\n", (unsigned long)tick++, on, !on);
    sleep_ms(250);
  }
}
