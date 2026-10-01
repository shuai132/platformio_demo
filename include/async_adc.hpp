#pragma once

#include <utility>

#include <coro/coro.hpp>

#include "adc_driver.hpp"

// Use an ISR-aware executor. Keep the driver and awaiting coroutine alive until completion.
inline coro::callback_awaiter<adc_result> capture_adc(adc_driver& adc, std::span<uint16_t> samples) {
  return coro::callback_awaiter<adc_result>([&adc, samples](adc_driver::completion_handler callback) {
    adc.capture_async(samples, std::move(callback));
  });
}
