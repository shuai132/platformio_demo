#include "led.h"

#include <Arduino.h>

#define LED_PIN_1 2
#define LED_PIN_2 3
#define LED_PIN_3 4
#define LED_PIN_4 5

#define LED_ON HIGH
#define LED_OFF LOW

void led_init() {
  pinMode(LED_PIN_1, OUTPUT);
  pinMode(LED_PIN_2, OUTPUT);
  pinMode(LED_PIN_3, OUTPUT);
  pinMode(LED_PIN_4, OUTPUT);
}

void led_on() {
  digitalWrite(LED_PIN_1, LED_ON);
  digitalWrite(LED_PIN_2, LED_ON);
  digitalWrite(LED_PIN_3, LED_ON);
  digitalWrite(LED_PIN_4, LED_ON);
}

void led_off() {
  digitalWrite(LED_PIN_1, LED_OFF);
  digitalWrite(LED_PIN_2, LED_OFF);
  digitalWrite(LED_PIN_3, LED_OFF);
  digitalWrite(LED_PIN_4, LED_OFF);
}
