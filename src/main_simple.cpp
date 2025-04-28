#ifdef MAIN_SIMPLE

#include <Arduino.h>

#include "log.h"

void setup() {
  DEBUG_SERIAL.begin(115200);
  pinMode(BLINK_LED, OUTPUT);
}

void loop() {
  static uint32_t count;
  LOG("loop: %u", count++);
  digitalWrite(BLINK_LED, HIGH);
  delay(100);
  digitalWrite(BLINK_LED, LOW);
  delay(100);
}

#endif
