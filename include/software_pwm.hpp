#pragma once
#include <coro/coro.hpp>

#include "cooperative_executor.hpp"

// Keep this object alive until every submitted run() coroutine has completed.
class software_pwm {
 public:
  static constexpr uint32_t duty_max = 1000;
  static constexpr uint32_t duty_ppm_max = 1000000;
  static constexpr uint32_t max_frequency_hz = 1000;
  software_pwm(uint8_t pin, uint32_t frequency_hz) : pin_(pin), frequency_hz_(frequency_hz) {}
  software_pwm(const software_pwm&) = delete;
  software_pwm& operator=(const software_pwm&) = delete;
  bool begin() {
    if (running_ || frequency_hz_ == 0 || frequency_hz_ > max_frequency_hz) return false;
    period_us_ = 1000000UL / frequency_hz_;
    pwm_platform::gpio_output(pin_);
    pwm_platform::gpio_write(pin_, false);
    stopped_ = false;
    return true;
  }
  // Per mille compatibility API, or parts per million for fine dark-end steps.
  void set_duty(uint16_t duty) {
    set_duty_ppm(uint32_t(duty > duty_max ? duty_max : duty) * 1000);
  }
  void set_duty_ppm(uint32_t duty) {
    duty_ppm_ = duty > duty_ppm_max ? duty_ppm_max : duty;
  }
  void stop() {
    stopped_ = true;
    duty_ppm_ = 0;
    if (period_us_ != 0) pwm_platform::gpio_write(pin_, false);
  }
  bool is_running() const {
    return running_;
  }
  coro::async<void> run(cooperative_executor& executor, uint32_t phase_us = 0) {
    if (period_us_ == 0 || stopped_ || running_) co_return;
    running_ = true;
    phase_us %= period_us_;
    auto start = executor.now_us() + phase_us;
    if (phase_us != 0) co_await executor.sleep_until_us(start);
    while (!stopped_) {
      const auto edge_now = executor.now_us();
      if (edge_now >= start + period_us_) start += ((edge_now - start) / period_us_) * period_us_;
      const auto width = uint64_t(period_us_) * duty_ppm_;
      const uint32_t high_us = width / duty_ppm_max;
      if constexpr (pwm_platform::has_precise_pulse_clock) {
        const uint32_t high_ticks = width * pwm_platform::pulse_ticks_per_us() / duty_ppm_max;
        const auto window = high_us < edge_guard_us ? edge_guard_us : high_us;
        const bool high = high_ticks != 0 && edge_now - start < window;
        const auto pulse_end = pwm_platform::pulse_clock32() + high_ticks;
        pwm_platform::gpio_write(pin_, high);
        if (high && duty_ppm_ != duty_ppm_max) {
          if (high_us > short_pulse_us) {
            co_await executor.sleep_until_us(edge_now + high_us - edge_guard_us);
            if (stopped_) break;
          }
          pwm_platform::wait_pulse_until(pulse_end);
          pwm_platform::gpio_write(pin_, false);
        }
      } else {
        const bool high = high_us != 0 && edge_now - start < high_us;
        pwm_platform::gpio_write(pin_, high);
        if (high && high_us != period_us_) {
          co_await executor.sleep_until_us(start + high_us);
          if (stopped_) break;
          pwm_platform::gpio_write(pin_, false);
        }
      }
      start += period_us_;
      const auto now = executor.now_us();
      if (start <= now) start += ((now - start) / period_us_ + 1) * period_us_;
      co_await executor.sleep_until_us(start);
    }
    pwm_platform::gpio_write(pin_, false);
    running_ = false;
  }

 private:
  // Short pulses stay in this coroutine; longer ones wake ahead of the edge.
  static constexpr uint32_t short_pulse_us = 32;
  static constexpr uint32_t edge_guard_us = 16;
  uint8_t pin_;
  uint32_t frequency_hz_;
  uint32_t period_us_ = 0;
  uint32_t duty_ppm_ = 0;
  bool stopped_ = false;
  bool running_ = false;
};
