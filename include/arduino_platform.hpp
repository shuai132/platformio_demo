#pragma once
#include <Arduino.h>

#include <cstdint>
namespace pwm_platform {
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
inline void delay_ms(uint32_t ms) {
  delay(ms);
}
inline void yield() {
  ::yield();
}
inline void console_begin() {
  Serial.begin(115200);
}
inline void log(const char* msg) {
  Serial.println(msg);
}
}  // namespace pwm_platform
