#ifdef MAIN_CORO

#include <Arduino.h>
#include <stm32f1xx_ll_adc.h>

#include <utility>

#include "async_adc.hpp"

async_adc adc;

bool async_adc::begin() {
  if (completion_) {
    return false;
  }
  initialized_ = false;
  HAL_NVIC_DisableIRQ(ADC1_2_IRQn);
  __HAL_RCC_ADC1_CLK_ENABLE();
  __HAL_RCC_ADC1_FORCE_RESET();
  __HAL_RCC_ADC1_RELEASE_RESET();
  __HAL_RCC_ADC_CONFIG(RCC_ADCPCLK2_DIV6);  // 72 MHz / 6 = 12 MHz.
  pinMode(PA0, INPUT_ANALOG);
  LL_ADC_SetDataAlignment(ADC1, LL_ADC_DATA_ALIGN_RIGHT);
  LL_ADC_SetSequencersScanMode(ADC1, LL_ADC_SEQ_SCAN_DISABLE);
  LL_ADC_REG_SetTriggerSource(ADC1, LL_ADC_REG_TRIG_SOFTWARE);
  LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_SINGLE);
  LL_ADC_REG_SetSequencerLength(ADC1, LL_ADC_REG_SEQ_SCAN_DISABLE);
  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_0);
  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_0, LL_ADC_SAMPLINGTIME_239CYCLES_5);
  LL_ADC_Enable(ADC1);
  delayMicroseconds(2);

  // Bounded synchronous calibration during setup, before spawning tasks.
  const uint32_t calibration_started = millis();
  SET_BIT(ADC1->CR2, ADC_CR2_RSTCAL);
  while (READ_BIT(ADC1->CR2, ADC_CR2_RSTCAL)) {
    if (static_cast<uint32_t>(millis() - calibration_started) >= 10) {
      LL_ADC_Disable(ADC1);
      return false;
    }
  }
  LL_ADC_StartCalibration(ADC1);
  while (LL_ADC_IsCalibrationOnGoing(ADC1)) {
    if (static_cast<uint32_t>(millis() - calibration_started) >= 10) {
      LL_ADC_Disable(ADC1);
      return false;
    }
  }
  completed_.store(false, std::memory_order_relaxed);
  LL_ADC_ClearFlag_EOS(ADC1);
  HAL_NVIC_ClearPendingIRQ(ADC1_2_IRQn);
  HAL_NVIC_SetPriority(ADC1_2_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(ADC1_2_IRQn);
  initialized_ = true;
  return true;
}

coro::callback_awaiter<async_adc::result> async_adc::read() {
  return coro::callback_awaiter<result>([this](std::function<void(result)> callback) {
    if (!initialized_ || completion_) {
      callback(std::nullopt);
      return;
    }
    // Register first: an IRQ may arrive immediately after starting conversion.
    completion_ = std::move(callback);
    completed_.store(false, std::memory_order_relaxed);
    started_ms_ = millis();
    LL_ADC_ClearFlag_EOS(ADC1);
    LL_ADC_EnableIT_EOS(ADC1);
    LL_ADC_REG_StartConversionSWStart(ADC1);
  });
}

void async_adc::on_interrupt() {
  if (LL_ADC_IsEnabledIT_EOS(ADC1) && LL_ADC_IsActiveFlag_EOS(ADC1)) {
    value_ = LL_ADC_REG_ReadConversionData12(ADC1);
    LL_ADC_ClearFlag_EOS(ADC1);
    LL_ADC_DisableIT_EOS(ADC1);
    // Publish to the main loop; no coroutine callbacks or queue operations here.
    completed_.store(true, std::memory_order_release);
  }
}

void async_adc::poll() {
  if (!completion_) {
    return;
  }
  result sample;
  if (completed_.load(std::memory_order_acquire)) {
    sample = value_;
  } else if (static_cast<uint32_t>(millis() - started_ms_) >= 10) {
    HAL_NVIC_DisableIRQ(ADC1_2_IRQn);
    LL_ADC_DisableIT_EOS(ADC1);
    LL_ADC_Disable(ADC1);
    HAL_NVIC_ClearPendingIRQ(ADC1_2_IRQn);
    initialized_ = false;  // Reinitialize before retrying after timeout.
  } else {
    return;
  }
  auto callback = std::move(completion_);
  completion_ = nullptr;
  // Clear ownership first: this callback may resume and start another read.
  callback(std::move(sample));
}

extern "C" void ADC1_2_IRQHandler() {
  adc.on_interrupt();
}

#endif
