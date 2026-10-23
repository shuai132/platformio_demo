#pragma once
#include <cstddef>
#include <cstdint>

// Application configuration only; PWM and animation remain independent.
namespace demo_config {
struct led_config {
  uint8_t pin;
  uint32_t pwm_hz;
  uint32_t breathing_ms;
  uint16_t max_brightness;
  uint32_t pwm_phase_us;
};
inline constexpr led_config leds[] = {
    {3, 1000, 2000, 300, 0},
    {4, 1000, 3000, 300, 333},
    {5, 1000, 4500, 300, 666},
};
inline constexpr size_t led_count = sizeof(leds) / sizeof(leds[0]);
}  // namespace demo_config
