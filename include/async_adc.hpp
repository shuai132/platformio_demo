#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>

#include <coro/coro.hpp>

// STM32F103: exclusively owns ADC1/channel 0 (PA0).
// Main-loop API; one pending read. Keep the awaiting coroutine alive until completion.
class async_adc {
 public:
  using result = std::optional<uint16_t>;
  bool begin();
  coro::callback_awaiter<result> read();
  void poll();
  void on_interrupt();

 private:
  std::function<void(result)> completion_;
  std::atomic<bool> completed_{false};
  uint16_t value_ = 0;
  uint32_t started_ms_ = 0;
  bool initialized_ = false;
};

extern async_adc adc;
