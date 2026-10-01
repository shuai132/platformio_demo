#pragma once

#include <Arduino.h>

#include <cstdint>
#include <deque>
#include <functional>
#include <utility>

#include <coro/executor.hpp>

// Single-threaded executor: use only from setup()/loop(), never from an ISR.
class arduino_executor : public coro::executor {
 public:
  void dispatch(std::function<void()> fn) override {
    if (!stopped_) {
      fn();
    }
  }

  void post(std::function<void()> fn) override {
    post_delayed_ns(std::move(fn), 0);
  }

  void post_delayed_ns(std::function<void()> fn, uint64_t delay_ns) override {
    if (!stopped_) {
      // Round up so a sub-millisecond delay does not resume early.
      const uint64_t delay_ms = delay_ns / 1000000 + (delay_ns % 1000000 != 0);
      tasks_.push_back({now_ms() + delay_ms, std::move(fn)});
    }
  }

  void stop() override {
    stopped_ = true;
  }

  void poll() {
    const uint64_t now = now_ms();
    if (stopped_) {
      return;
    }
    for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
      if (it->ready_ms <= now) {
        auto fn = std::move(it->fn);
        tasks_.erase(it);
        // Remove before resuming: the coroutine may enqueue its next delay.
        fn();
        return;
      }
    }
  }

 private:
  uint64_t now_ms() {
    const uint32_t tick = millis();
    // Extend the wrapping millis() counter; poll at least once per wrap period.
    elapsed_ms_ += static_cast<uint32_t>(tick - last_tick_);
    last_tick_ = tick;
    return elapsed_ms_;
  }

  struct pending_task {
    uint64_t ready_ms;
    std::function<void()> fn;
  };

  std::deque<pending_task> tasks_;
  uint64_t elapsed_ms_ = 0;
  uint32_t last_tick_ = 0;
  bool stopped_ = false;
};
