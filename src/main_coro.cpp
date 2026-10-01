#ifdef MAIN_CORO

#include <Arduino.h>

#include <coro/coro.hpp>
#include <coro/time.hpp>

#include "arduino_executor.hpp"
#include "async_adc.hpp"

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

coro::async<void> sample_adc() {
  for (;;) {
    auto value = co_await adc.read();
    if (value) {
      Serial.print("ADC PA0: ");
      Serial.println(*value);
    } else {
      Serial.println("ADC read failed; reinitializing");
      if (!adc.begin()) {
        Serial.println("ADC initialization failed");
        co_return;
      }
    }
    co_await coro::delay(500);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting coroutine blink...");
  const bool adc_ready = adc.begin();
  coro::spawn(executor, blink());
  coro::spawn(executor, heartbeat());
  if (adc_ready) {
    coro::spawn(executor, sample_adc());
  } else {
    Serial.println("ADC initialization failed");
  }
}

void loop() {
  adc.poll();
  executor.poll();
}

#endif
