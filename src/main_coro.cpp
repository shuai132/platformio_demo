#ifdef MAIN_CORO

#include <Arduino.h>

#include <array>

#include <coro/coro.hpp>
#include <coro/time.hpp>

#include "stm32_arduino_executor.hpp"
#define ADC_DRIVER_IMPLEMENTATION
#include "async_adc.hpp"

constexpr uint32_t ADC_SAMPLE_RATE_HZ = 100;
constexpr size_t ADC_SAMPLE_COUNT = 64;

stm32_arduino_executor executor;

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
  std::array<uint16_t, ADC_SAMPLE_COUNT> samples{};
  for (;;) {
    // ADC_SAMPLE_COUNT / ADC_SAMPLE_RATE_HZ: defaults to 64 / 100 = 640 ms.
    auto result = co_await capture_adc(adc, samples);
    if (result) {
      Serial.printf("ADC: %u\n", static_cast<unsigned>(samples.back()));
    } else {
      Serial.println("ADC capture failed");
      co_return;
    }
    // co_await coro::delay(500);  // Default sampling configuration already takes approximately 640 ms.
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting coroutine blink...");
  const bool adc_ready = adc.initialize(ADC_SAMPLE_RATE_HZ);
  coro::spawn(executor, blink());
  coro::spawn(executor, heartbeat());
  if (adc_ready) {
    coro::spawn(executor, sample_adc());
  } else {
    Serial.println("ADC initialization failed");
  }
}

void loop() {
  executor.poll();
  executor.sleep_if_idle();
}

#endif
