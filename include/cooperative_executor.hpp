#pragma once
#include <coro/executor.hpp>
#include <coroutine>
#include <cstdint>
#include <functional>
#include <list>
#include <utility>

#include "platform_io.hpp"

// Single owner only: bare metal polls directly; Arduino ESP32 uses a timer task.
class cooperative_executor : public coro::executor {
 public:
  cooperative_executor() = default;
  cooperative_executor(const cooperative_executor&) = delete;
  cooperative_executor& operator=(const cooperative_executor&) = delete;

  // Intrusive timer nodes live in coroutine frames; waits allocate no queue nodes.
  class timer_awaiter {
   public:
    timer_awaiter(cooperative_executor& owner, uint64_t deadline) : owner_(owner), deadline_us_(deadline) {}
    timer_awaiter(const timer_awaiter&) = delete;
    timer_awaiter& operator=(const timer_awaiter&) = delete;
    ~timer_awaiter() {
      if (pending_) {
        auto** link = &owner_.timers_;
        while (*link != this) link = &(*link)->next_;
        *link = next_;
      }
    }
    bool await_ready() const noexcept {
      return false;
    }
    void await_suspend(std::coroutine_handle<> handle) noexcept {
      handle_ = handle;
      auto** link = &owner_.timers_;
      while (*link && (*link)->deadline_us_ <= deadline_us_) link = &(*link)->next_;
      next_ = *link;
      *link = this;
      pending_ = true;
    }
    void await_resume() const noexcept {}

   private:
    friend class cooperative_executor;
    cooperative_executor& owner_;
    uint64_t deadline_us_;
    std::coroutine_handle<> handle_;
    timer_awaiter* next_ = nullptr;
    bool pending_ = false;
  };
  timer_awaiter sleep_until_us(uint64_t deadline) {
    return {*this, deadline};
  }
  uint64_t next_deadline_us() const {
    if (stopped_) return UINT64_MAX;
    auto deadline = tasks_.empty() ? UINT64_MAX : tasks_.front().deadline_us;
    if (timers_ && timers_->deadline_us_ < deadline) deadline = timers_->deadline_us_;
    return deadline;
  }
  void dispatch(std::function<void()> fn) override {
    post(std::move(fn));
  }
  void post(std::function<void()> fn) override {
    post_delayed_ns(std::move(fn), 0);
  }
  void post_delayed_ns(std::function<void()> fn, uint64_t ns) override {
    if (stopped_ || !fn) return;
    const auto deadline = now_us() + ns / 1000 + (ns % 1000 != 0);
    auto it = tasks_.begin();
    while (it != tasks_.end() && it->deadline_us <= deadline) ++it;
    tasks_.insert(it, {deadline, std::move(fn)});
  }
  void poll() {
    const auto now = now_us();
    if (stopped_) return;
    if (timers_ && timers_->deadline_us_ <= now && (tasks_.empty() || timers_->deadline_us_ <= tasks_.front().deadline_us)) {
      auto* timer = timers_;
      timers_ = timer->next_;
      timer->pending_ = false;
      const auto handle = timer->handle_;
      handle.resume();
      return;
    }
    if (!tasks_.empty() && tasks_.front().deadline_us <= now) {
      auto fn = std::move(tasks_.front().fn);
      tasks_.pop_front();
      fn();
    }
  }
  void sleep_if_idle(uint32_t max_sleep_ms = 10) {
    if (stopped_ || !max_sleep_ms) return;
    const auto now = now_us();
    auto wait = uint64_t(max_sleep_ms) * 1000;
    const auto deadline = next_deadline_us();
    if (deadline <= now) return;
    if (deadline - now < wait) wait = deadline - now;
    if (wait >= 1000)
      pwm_platform::delay_ms(static_cast<uint32_t>(wait / 1000));
    else
      pwm_platform::yield();
  }
  // Stops dispatch only; suspended coroutines and their objects must stay alive.
  void stop() {
    stopped_ = true;
  }
  uint64_t now_us() {
    const auto tick = pwm_platform::micros32();
    elapsed_us_ += static_cast<uint32_t>(tick - last_tick_);
    last_tick_ = tick;
    return elapsed_us_;
  }

 private:
  struct task {
    uint64_t deadline_us;
    std::function<void()> fn;
  };
  uint32_t last_tick_ = pwm_platform::micros32();
  uint64_t elapsed_us_ = 0;
  std::list<task> tasks_;
  timer_awaiter* timers_ = nullptr;
  bool stopped_ = false;
};
