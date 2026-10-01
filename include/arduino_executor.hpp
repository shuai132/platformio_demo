#pragma once

#include <Arduino.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <exception>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include <coro/executor.hpp>

// Timers run in the main loop; dispatch/post from an ISR defer work to poll().
class arduino_executor : public coro::executor {
 public:
  void dispatch(std::function<void()> fn) override {
    if (stopped_) {
      return;
    }
    if (__get_IPSR() != 0) {
      enqueue_interrupt(std::move(fn));
    } else {
      fn();
    }
  }

  void post(std::function<void()> fn) override {
    if (stopped_) {
      return;
    }
    if (__get_IPSR() != 0) {
      enqueue_interrupt(std::move(fn));
    } else {
      post_delayed_ns(std::move(fn), 0);
    }
  }

  void post_delayed_ns(std::function<void()> fn, uint64_t delay_ns) override {
    if (!stopped_) {
      // Round up so a sub-millisecond delay does not resume early.
      const uint64_t delay_ms = delay_ns / 1000000 + (delay_ns % 1000000 != 0);
      tasks_.push_back({now_ms() + delay_ms, next_sequence_++, std::move(fn)});
      std::push_heap(tasks_.begin(), tasks_.end(), later_task{});
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
    // Drain one IRQ completion, then give due timers a turn as well.
    std::function<void()> ready;
    const uint32_t interrupt_mask = __get_PRIMASK();
    __disable_irq();
    if (isr_read_ != isr_write_) {
      ready = std::move(isr_tasks_[isr_read_]);
      isr_read_ = (isr_read_ + 1) % isr_tasks_.size();
    }
    __set_PRIMASK(interrupt_mask);
    if (ready) {
      ready();
    }
    if (stopped_) {
      return;
    }
    if (!tasks_.empty() && tasks_.front().ready_ms <= now) {
      std::pop_heap(tasks_.begin(), tasks_.end(), later_task{});
      auto fn = std::move(tasks_.back().fn);
      tasks_.pop_back();
      // Remove before resuming: the coroutine may enqueue its next delay.
      fn();
    }
  }

 private:
  void enqueue_interrupt(std::function<void()> fn) {
    const uint32_t mask = __get_PRIMASK();
    __disable_irq();
    const size_t next = (isr_write_ + 1) % isr_tasks_.size();
    if (next == isr_read_) {
      __set_PRIMASK(mask);
      std::terminate();  // Never silently discard a coroutine continuation.
    }
    isr_tasks_[isr_write_] = std::move(fn);
    isr_write_ = next;
    __set_PRIMASK(mask);
  }

  // Fixed ISR mailbox; moving functions does not allocate. Capacity: eight.
  std::array<std::function<void()>, 9> isr_tasks_{};
  size_t isr_read_ = 0;
  size_t isr_write_ = 0;

  uint64_t now_ms() {
    const uint32_t tick = millis();
    // Extend the wrapping millis() counter; poll at least once per wrap period.
    elapsed_ms_ += static_cast<uint32_t>(tick - last_tick_);
    last_tick_ = tick;
    return elapsed_ms_;
  }

  struct pending_task {
    uint64_t ready_ms;
    uint64_t sequence;
    std::function<void()> fn;
  };

  struct later_task {
    bool operator()(const pending_task& lhs, const pending_task& rhs) const {
      if (lhs.ready_ms != rhs.ready_ms) {
        return lhs.ready_ms > rhs.ready_ms;
      }
      return lhs.sequence > rhs.sequence;
    }
  };

  // Min-heap: earliest deadline first, FIFO for matching deadlines.
  std::vector<pending_task> tasks_;
  uint64_t next_sequence_ = 0;
  uint64_t elapsed_ms_ = 0;
  uint32_t last_tick_ = 0;
  std::atomic<bool> stopped_{false};
};
