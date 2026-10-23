#pragma once
#include <coro/coro.hpp>
#include <cstdint>

#include "software_pwm.hpp"

// Brightness animation is independent of GPIO timing.
class breathing_led {
 public:
  breathing_led(software_pwm& pwm, uint32_t period_ms, uint16_t max_brightness = 300) : pwm_(pwm), period_us_(uint64_t(period_ms) * 1000) {
    set_max_brightness(max_brightness);
  }
  breathing_led(const breathing_led&) = delete;
  breathing_led& operator=(const breathing_led&) = delete;
  void set_max_brightness(uint16_t value) {
    max_brightness_ = value > 1000 ? 1000 : value;
  }
  coro::async<void> run(cooperative_executor& executor) {
    pwm_.set_duty(0);
    if (period_us_ == 0) co_return;
    const auto start = executor.now_us();
    auto next_update = start;
    for (;;) {
      const auto phase = (executor.now_us() - start) % period_us_;
      const auto ramp = phase <= period_us_ / 2 ? phase : period_us_ - phase;
      const uint32_t brightness_q16 = ramp * 131072 / period_us_;
      const auto duty = (uint64_t(brightness_q16) * brightness_q16 * max_brightness_ * 1000) >> 32;
      pwm_.set_duty_ppm(static_cast<uint32_t>(duty));
      const auto now = executor.now_us();
      next_update += update_interval_us;
      if (next_update <= now) next_update += ((now - next_update) / update_interval_us + 1) * update_interval_us;
      co_await executor.sleep_until_us(next_update);
    }
  }

 private:
  static constexpr uint32_t update_interval_us = 1000;
  software_pwm& pwm_;
  uint64_t period_us_;
  uint16_t max_brightness_;
};
