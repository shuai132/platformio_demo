#ifdef MAIN_CORO

#include <Arduino.h>

#include <coro/coro.hpp>
#include <coro/time.hpp>

#include "arduino_executor.hpp"

arduino_executor executor;

coro::async<void> blink() {
  pinMode(BLINK_LED, OUTPUT);
  for (;;) {
    digitalWrite(BLINK_LED, LOW);  // PC13 LED is active-low.
    co_await coro::delay(100);
    digitalWrite(BLINK_LED, HIGH);
    co_await coro::delay(100);
  }
}

coro::async<void> heartbeat() {
  for (;;) {
    co_await coro::delay(1000);
    Serial.print("Heartbeat, uptime: ");
    Serial.print(millis());
    Serial.println(" ms");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting coroutine blink...");
  coro::spawn(executor, blink());
  coro::spawn(executor, heartbeat());
}

void loop() {
  executor.poll();
}

#endif
