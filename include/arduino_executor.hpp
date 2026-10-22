#pragma once

#include <Arduino.h>
#include "timed_executor.hpp"

// Portable Arduino driver, used by ESP8266. All access stays on the loop task.
class arduino_executor : public timed_executor {
 public:
  void poll() { poll_one(); }

  uint64_t now_us() override {
    const auto tick = static_cast<uint32_t>(micros());
    elapsed_us_ += static_cast<uint32_t>(tick - last_tick_);
    last_tick_ = tick;
    return elapsed_us_;
  }

  void sleep_if_idle(uint32_t max_sleep_ms = 10) {
    if (is_stopped() || max_sleep_ms == 0) {
      return;
    }
    const auto deadline = next_deadline_us();
    const auto now = now_us();
    uint64_t wait_us = static_cast<uint64_t>(max_sleep_ms) * 1000;
    if (deadline) {
      if (*deadline <= now) {
        return;
      }
      if (*deadline - now < wait_us) {
        wait_us = *deadline - now;
      }
    }
    // Leave short waits to loop polling; reserve 1 ms for delay() jitter.
    if (wait_us >= 2000) {
      delay(static_cast<uint32_t>((wait_us - 1000) / 1000));
    }
  }

 private:
  // Poll at least once per micros() wrap (~71 minutes) to extend the clock.
  uint32_t last_tick_ = static_cast<uint32_t>(micros());
  uint64_t elapsed_us_ = 0;
};
