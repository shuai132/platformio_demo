#pragma once

#include <Arduino.h>
#include <coro/coro.hpp>
#include <cstdint>

#include "timed_executor.hpp"

// Executor task only. Keep this object alive while run() executes; spawn it once.
template <uint32_t FrequencyHz>
class software_pwm {
  static_assert(FrequencyHz > 0 && FrequencyHz <= 1000000);
  static_assert(1000000 % FrequencyHz == 0,
                "PWM period must be a whole number of microseconds");

 public:
  static constexpr uint32_t period_us = 1000000 / FrequencyHz;
  static constexpr uint16_t duty_max = 1000;

  software_pwm(timed_executor& executor, uint8_t pin)
      : executor_(executor), pin_(pin) {}
  software_pwm(const software_pwm&) = delete;
  software_pwm& operator=(const software_pwm&) = delete;

  // Per mille, clamped to [0, 1000]. Applied on the next PWM cycle.
  void set_duty(uint16_t duty) {
    duty_ = duty > duty_max ? duty_max : duty;
  }

  coro::async<void> run() {
    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, LOW);
    const auto origin = executor_.now_us();
    uint64_t previous_cycle = UINT64_MAX;
    uint32_t on_us = 0;
    bool high = false;
    for (;;) {
      const auto elapsed = executor_.now_us() - origin;
      const auto cycle = elapsed / period_us;
      const auto phase = static_cast<uint32_t>(elapsed % period_us);
      if (cycle != previous_cycle) {
        on_us = period_us * static_cast<uint32_t>(duty_) / duty_max;
        previous_cycle = cycle;
      }
      const bool next_high = phase < on_us;
      if (next_high != high) {
        digitalWrite(pin_, next_high ? HIGH : LOW);
        high = next_high;
      }
      // Skip missed edges after a late resume rather than emitting bursts
      // or shifting all subsequent periods.
      const auto next_edge = next_high && on_us < period_us ? on_us : period_us;
      co_await executor_.sleep_until_us(origin + cycle * period_us + next_edge);
    }
  }

 private:
  timed_executor& executor_;
  const uint8_t pin_;
  uint16_t duty_ = 0;
};
