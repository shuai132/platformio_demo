#pragma once

#include <coro/coro.hpp>
#include <cstdint>

#include "timed_executor.hpp"

// Animation depends on duty control, not GPIO. Pwm exposes duty_max and
// set_duty(). Keep both objects alive while run() executes; spawn it once.
template <typename Pwm>
class breathing_led {
 public:
  breathing_led(timed_executor& executor, Pwm& pwm,
                uint32_t cycle_ms = 3000, uint32_t phase_ms = 0,
                uint16_t max_duty = Pwm::duty_max, uint16_t min_duty = 30)
      : executor_(executor), pwm_(pwm),
        cycle_us_(static_cast<uint64_t>(cycle_ms < 20 ? 20 : cycle_ms) * 1000),
        phase_us_(static_cast<uint64_t>(phase_ms) * 1000 % cycle_us_),
        max_duty_(max_duty > Pwm::duty_max ? Pwm::duty_max : max_duty),
        min_duty_(min_duty > max_duty_ ? max_duty_ : min_duty) {}
  breathing_led(const breathing_led&) = delete;
  breathing_led& operator=(const breathing_led&) = delete;

  coro::async<void> run() {
    const auto origin = executor_.now_us();
    for (;;) {
      const auto elapsed = executor_.now_us() - origin;
      const auto phase = (elapsed + phase_us_) % cycle_us_;
      const auto ramp = phase <= cycle_us_ / 2 ? phase : cycle_us_ - phase;
      const auto brightness = ramp * Pwm::duty_max * 2 / cycle_us_;
      // Keep a visible floor while scaling the corrected curve to its peak.
      pwm_.set_duty(static_cast<uint16_t>(min_duty_ + brightness * brightness *
                                         (max_duty_ - min_duty_) /
                                         (uint64_t{Pwm::duty_max} * Pwm::duty_max)));
      co_await executor_.sleep_until_us(origin + (elapsed / update_us + 1) * update_us);
    }
  }

 private:
  static constexpr uint32_t update_us = 10000;
  timed_executor& executor_;
  Pwm& pwm_;
  const uint64_t cycle_us_;
  const uint64_t phase_us_;
  const uint16_t max_duty_;
  const uint16_t min_duty_;
};
