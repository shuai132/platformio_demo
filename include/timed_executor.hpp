#pragma once

#include <coro/coro.hpp>
#include <cstdint>
#include <functional>
#include <list>
#include <optional>
#include <utility>

// Platform-independent scheduling. All access belongs to one execution task.
// Drivers supply a monotonic microsecond clock and invoke ready callbacks.
class timed_executor : public coro::executor {
 public:
  timed_executor() = default;
  timed_executor(const timed_executor&) = delete;
  timed_executor& operator=(const timed_executor&) = delete;

  virtual uint64_t now_us() = 0;

  void dispatch(std::function<void()> fn) override {
    // Queue even on the executor task to avoid recursive coroutine resumes.
    post(std::move(fn));
  }

  void post(std::function<void()> fn) override {
    post_delayed_ns(std::move(fn), 0);
  }

  void post_delayed_ns(std::function<void()> fn, uint64_t delay_ns) override {
    if (stopped_ || !fn) {
      return;
    }
    // Round up to the scheduler's microsecond resolution.
    const auto delay_us = delay_ns / 1000 + (delay_ns % 1000 != 0);
    tasks_.push_back({now_us() + delay_us, std::move(fn)});
  }

  // Absolute deadlines avoid accumulating coroutine/dispatch overhead.
  auto sleep_until_us(uint64_t deadline_us) {
    return coro::callback_awaiter<void>([this, deadline_us](auto, auto callback) {
      if (!stopped_) {
        tasks_.push_back({deadline_us, std::move(callback)});
      }
    });
  }

  // Stops dispatch; suspended coroutines remain alive. Call from the owner task.
  void stop() { stopped_ = true; }

 protected:
  bool is_stopped() const { return stopped_; }

  bool poll_one() {
    const auto now = now_us();
    if (stopped_) {
      return false;
    }
    // Run one ready callback per poll, in insertion order. Remove it before
    // invoking user code, which may enqueue more callbacks.
    for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
      if (it->deadline_us <= now) {
        auto fn = std::move(it->fn);
        tasks_.erase(it);
        fn();
        return true;
      }
    }
    return false;
  }

  std::optional<uint64_t> next_deadline_us() const {
    if (stopped_ || tasks_.empty()) {
      return std::nullopt;
    }
    uint64_t deadline = UINT64_MAX;
    for (const auto& task : tasks_) {
      if (task.deadline_us < deadline) {
        deadline = task.deadline_us;
      }
    }
    return deadline;
  }

 private:
  struct task {
    uint64_t deadline_us;
    std::function<void()> fn;
  };

  std::list<task> tasks_;
  bool stopped_ = false;
};
