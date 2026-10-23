#pragma once
#include <esp_timer.h>

#include "cooperative_executor.hpp"

// Program lifetime, single owner: ESP_TIMER_TASK. User callbacks must not block.
class esp32_executor_runner {
 public:
  explicit esp32_executor_runner(cooperative_executor& executor) : executor_(executor) {}
  esp32_executor_runner(const esp32_executor_runner&) = delete;
  esp32_executor_runner& operator=(const esp32_executor_runner&) = delete;
  bool start() {
    if (timer_) return false;
    esp_timer_create_args_t args{};
    args.callback = [](void* arg) {
      static_cast<esp32_executor_runner*>(arg)->tick();
    };
    args.arg = this;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "coro_pwm";
    if (esp_timer_create(&args, &timer_) != ESP_OK) return false;
    if (esp_timer_start_once(timer_, 1) != ESP_OK) {
      esp_timer_delete(timer_);
      timer_ = nullptr;
      return false;
    }
    return true;
  }

 private:
  static constexpr uint32_t callback_budget_us = 100;
  static constexpr uint32_t spin_window_us = 25;
  void tick() {
    const auto entered = executor_.now_us();
    for (;;) {
      const auto deadline = executor_.next_deadline_us();
      if (deadline == UINT64_MAX) return;
      const auto now = executor_.now_us();
      if (now - entered >= callback_budget_us || (deadline > now && deadline - now > spin_window_us)) {
        const auto remaining = deadline > now ? deadline - now : 1;
        const auto wake_in = remaining > spin_window_us ? remaining - spin_window_us : 1;
        ESP_ERROR_CHECK(esp_timer_start_once(timer_, wake_in));
        return;
      }
      if (deadline <= now) executor_.poll();
    }
  }
  cooperative_executor& executor_;
  esp_timer_handle_t timer_ = nullptr;
};
