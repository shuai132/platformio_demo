#include <Arduino.h>

#include <coro/coro.hpp>
#include <coro/time.hpp>

#if defined(ARDUINO_ARCH_ESP32)
#include "esp32_executor.hpp"
#else
#include "arduino_executor.hpp"
#endif

namespace {

// ESP32 transfers ownership to the timer task after setup.
#if defined(ARDUINO_ARCH_ESP32)
esp32_executor executor;
#else
arduino_executor executor;
#endif

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
#if defined(ARDUINO_ARCH_ESP32)
  executor.start();
#endif
}

void loop() {
#if defined(ARDUINO_ARCH_ESP32)
  delay(1000);
#else
  executor.poll();
  executor.sleep_if_idle();
#endif
}
