#include "led.h"

#include <Arduino.h>

#define LED_PIN_1 2
#define LED_PIN_2 3
#define LED_PIN_3 4
#define LED_PIN_4 5

#define LED_ON HIGH
#define LED_OFF LOW

static LEDState ledState;

void ledSetState(LEDState state) {
  ledState = state;
}

LEDState ledGetState() {
  return ledState;
}

void ledInit() {
  pinMode(LED_PIN_1, OUTPUT);
  pinMode(LED_PIN_2, OUTPUT);
  pinMode(LED_PIN_3, OUTPUT);
  pinMode(LED_PIN_4, OUTPUT);
}

void ledOn() {
  if (ledState.led1) {
    digitalWrite(LED_PIN_1, LED_ON);
  }
  if (ledState.led2) {
    digitalWrite(LED_PIN_2, LED_ON);
  }
  if (ledState.led3) {
    digitalWrite(LED_PIN_3, LED_ON);
  }
  if (ledState.led4) {
    digitalWrite(LED_PIN_4, LED_ON);
  }
}

void ledOff() {
  digitalWrite(LED_PIN_1, LED_OFF);
  digitalWrite(LED_PIN_2, LED_OFF);
  digitalWrite(LED_PIN_3, LED_OFF);
  digitalWrite(LED_PIN_4, LED_OFF);
}
