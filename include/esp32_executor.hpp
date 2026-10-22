#pragma once

#include <esp_timer.h>
#include "timed_executor.hpp"

// Queue startup work before start(); afterwards only executor callbacks may
// access this object or its coroutines. No cross-task or ISR access is allowed.
// Keep the executor alive for the lifetime of its detached coroutines/timer.
class esp32_executor : public timed_executor {
 public:
  uint64_t now_us() override {
    return static_cast<uint64_t>(esp_timer_get_time());
  }

  // Call once, as the final executor operation in setup(). ESP_TIMER_TASK uses
  // ESP-IDF's high-priority timer task; callbacks must be short and nonblocking.
  void start() {
    esp_timer_create_args_t args{};
    args.callback = [](void* self) {
      static_cast<esp32_executor*>(self)->on_timer();
    };
    args.arg = this;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "coro";
    ESP_ERROR_CHECK(esp_timer_create(&args, &timer_));
    ESP_ERROR_CHECK(esp_timer_start_once(timer_, 1));
  }

 private:
  void on_timer() {
    // Bound work so immediate-post loops cannot monopolize the shared task.
    for (unsigned i = 0; i < callback_budget && poll_one(); ++i) {}
    if (const auto deadline = next_deadline_us()) {
      const auto now = now_us();
      ESP_ERROR_CHECK(esp_timer_start_once(timer_, *deadline > now ? *deadline - now : 1));
    }
  }

  static constexpr unsigned callback_budget = 32;
  esp_timer_handle_t timer_ = nullptr;
};
