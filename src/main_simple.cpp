#ifdef MAIN_SIMPLE

#include <Arduino.h>

#include "log.h"

void setup() {
  Serial1.begin(115200);
  pinMode(BLINK_LED, OUTPUT);
}

void loop() {
  LOG("loop...");
  digitalWrite(BLINK_LED, HIGH);
  delay(100);
  digitalWrite(BLINK_LED, LOW);
  delay(100);
}

#endif
