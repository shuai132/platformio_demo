#include <Arduino.h>

#include <coro/coro.hpp>
#include <coro/time.hpp>

#include "arduino_executor.hpp"

namespace {

// Only the Arduino loop task polls this executor.
arduino_executor executor;

coro::async<void> blink() {
  for (;;) {
    digitalWrite(BLINK_LED, HIGH);
    co_await coro::delay(100);
    digitalWrite(BLINK_LED, LOW);
    co_await coro::delay(100);
  }
}

coro::async<void> heartbeat() {
  unsigned long count = 0;
  for (;;) {
    Serial.printf("coro heartbeat: %lu\n", count++);
    co_await coro::delay(1000);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  pinMode(BLINK_LED, OUTPUT);
  coro::spawn(executor, blink());
  coro::spawn(executor, heartbeat());
}

void loop() {
  executor.poll();
  delay(1);
}
