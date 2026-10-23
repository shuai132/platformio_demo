#pragma once
#include <cstdint>
namespace pwm_platform {
inline constexpr bool has_precise_pulse_clock = true;
inline uint32_t pulse_clock_mhz = 0;
inline uint32_t pulse_clock32() {
  uint32_t ticks;
  asm volatile("csrr %0, 0x7e2" : "=r"(ticks));
  return ticks;
}
inline uint32_t pulse_ticks_per_us() {
  return pulse_clock_mhz;
}
inline void wait_pulse_until(uint32_t deadline) {
  while (static_cast<int32_t>(pulse_clock32() - deadline) < 0) {
  }
}
inline volatile uint32_t& reg(uintptr_t address) {
  return *reinterpret_cast<volatile uint32_t*>(address);
}
inline uint32_t micros32() {
  constexpr uintptr_t systimer = 0x60023000;
  reg(systimer + 0x04) = 1U << 30;
  while ((reg(systimer + 0x04) & (1U << 29)) == 0) {
  }
  const auto high = reg(systimer + 0x40) & 0xfffff;
  const auto low = reg(systimer + 0x44);
  return static_cast<uint32_t>(((uint64_t(high) << 32) | low) >> 4);
}
inline void gpio_write(uint8_t pin, bool high) {
  reg(high ? 0x60004008 : 0x6000400c) = 1U << pin;
}
inline void gpio_output(uint8_t pin) {
  gpio_write(pin, false);
  auto& mux = reg(0x60009004 + 4U * pin);
  mux = (mux & ~((7U << 12) | (1U << 7) | (1U << 8))) | (1U << 12);
  reg(0x60004554 + 4U * pin) = 128 | (1U << 9);
  reg(0x60004024) = 1U << pin;
}
}  // namespace pwm_platform
