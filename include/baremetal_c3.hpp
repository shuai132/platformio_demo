#pragma once
#include "esp32c3_io.hpp"
namespace pwm_platform {
inline void delay_ms(uint32_t ms) {
  while (ms--) {
    const auto start = micros32();
    while (static_cast<uint32_t>(micros32() - start) < 1000) {
    }
  }
}
inline void yield() {}
inline void console_begin() {}
void log(const char* message);
}  // namespace pwm_platform
