#ifdef MAIN_SIMPLE

#include <Arduino.h>

#include "log.h"

void setup() {
  DEBUG_SERIAL.begin(DEBUG_BAUDRATE);
  pinMode(BLINK_LED, OUTPUT);
}

void loop() {
  static uint32_t count;
  LOG("loop: %u", count++);
  LOG("hex: 0x%02X", 0x01);
  LOG("float: %f", PI);
  printf("printf\n");

  digitalWrite(BLINK_LED, HIGH);
  delay(100);
  digitalWrite(BLINK_LED, LOW);
  delay(100);
}

#endif
