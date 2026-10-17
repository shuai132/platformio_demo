#pragma once

#include <Arduino.h>

#include <coro/executor.hpp>
#include <cstdint>
#include <functional>
#include <list>
#include <utility>

// Single-threaded: call every method from the Arduino loop task, never an ISR.
class arduino_executor : public coro::executor {
 public:
  arduino_executor() = default;
  arduino_executor(const arduino_executor&) = delete;
  arduino_executor& operator=(const arduino_executor&) = delete;

  void dispatch(std::function<void()> fn) override {
    // Queue even on the loop task to avoid recursive coroutine resumes.
    post(std::move(fn));
  }

  void post(std::function<void()> fn) override {
    post_delayed_ns(std::move(fn), 0);
  }

  void post_delayed_ns(std::function<void()> fn, uint64_t delay_ns) override {
    if (stopped_ || !fn) {
      return;
    }
    // Round up to the Arduino clock's millisecond resolution.
    const auto delay_ms = delay_ns / 1000000 + (delay_ns % 1000000 != 0);
    tasks_.push_back({now_ms() + delay_ms, std::move(fn)});
  }

  void poll() {
    const auto now = now_ms();
    if (stopped_) {
      return;
    }
    // Run one ready callback per poll, in insertion order. Remove it before
    // invoking user code, which may enqueue more callbacks.
    for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
      if (it->deadline_ms <= now) {
        auto fn = std::move(it->fn);
        tasks_.erase(it);
        fn();
        return;
      }
    }
  }

  // Let Arduino background work run only when no callback is ready. Bound the
  // wait by both the next timer and a short maximum for loop responsiveness.
  // This yields execution via delay(); it does not enter hardware deep sleep.
  void sleep_if_idle(uint32_t max_sleep_ms = 10) {
    if (stopped_ || max_sleep_ms == 0) {
      return;
    }
    const auto now = now_ms();
    uint64_t wait_ms = max_sleep_ms;
    for (const auto& task : tasks_) {
      if (task.deadline_ms <= now) {
        return;
      }
      const auto remaining = task.deadline_ms - now;
      if (remaining < wait_ms) {
        wait_ms = remaining;
      }
    }
    delay(static_cast<uint32_t>(wait_ms));
  }

  // Stopping does not cancel or destroy suspended coroutines.
  void stop() {
    stopped_ = true;
  }

 private:
  struct task {
    uint64_t deadline_ms;
    std::function<void()> fn;
  };

  uint64_t now_ms() {
    const auto tick = static_cast<uint32_t>(millis());
    elapsed_ms_ += static_cast<uint32_t>(tick - last_tick_);
    last_tick_ = tick;
    return elapsed_ms_;
  }

  // Poll at least once per millis() wrap (~49 days) to extend the clock.
  uint32_t last_tick_ = static_cast<uint32_t>(millis());
  uint64_t elapsed_ms_ = 0;
  std::list<task> tasks_;
  bool stopped_ = false;
};
