#pragma once
#include <Arduino.h>

#include <cstdint>
#if defined(CONFIG_IDF_TARGET_ESP32C3)
#include "esp32c3_io.hpp"
#endif
namespace pwm_platform {
#if !defined(CONFIG_IDF_TARGET_ESP32C3)
inline constexpr bool has_precise_pulse_clock = false;
inline uint32_t micros32() {
  return static_cast<uint32_t>(micros());
}
inline uint32_t pulse_clock32() {
  return micros32();
}
inline uint32_t pulse_ticks_per_us() {
  return 1;
}
inline void wait_pulse_until(uint32_t deadline) {
  while (static_cast<int32_t>(pulse_clock32() - deadline) < 0) {
  }
}
inline void gpio_output(uint8_t pin) {
  pinMode(pin, OUTPUT);
}
inline void gpio_write(uint8_t pin, bool high) {
  digitalWrite(pin, high ? HIGH : LOW);
}
#endif
inline void delay_ms(uint32_t ms) {
  delay(ms);
}
inline void yield() {
  ::yield();
}
inline void console_begin() {
  Serial.begin(115200);
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  // Same CPU rate and performance-counter clock as the bare-metal environment.
  if (getCpuFrequencyMhz() != 40) {
    Serial.println("PWM comparison requires a 40 MHz CPU");
    abort();
  }
  asm volatile("csrw 0x7e0, %0\ncsrw 0x7e1, %0" ::"r"(1U));
  pulse_clock_mhz = getCpuFrequencyMhz();
  Serial.println("Arduino C3: shared PWM/animation, CPU 40 MHz");
#endif
}
inline void log(const char* msg) {
  Serial.println(msg);
}
}  // namespace pwm_platform
