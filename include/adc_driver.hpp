#pragma once

#include <Arduino.h>
#include <stm32f1xx_ll_adc.h>
#include <stm32f1xx_ll_dma.h>
#include <stm32f1xx_ll_tim.h>

#include <cstdint>
#include <expected>
#include <functional>
#include <span>
#include <utility>

enum class adc_error { not_initialized, busy, invalid_buffer, dma_transfer };
using adc_result = std::expected<void, adc_error>;

// STM32F103: exclusively owns ADC1/channel 0 (PA0), TIM3 and DMA1 channel 1. Use the global adc instance.
// Call initialize/capture_async from the main loop. Completion runs in DMA IRQ context.
class adc_driver {
 public:
  using completion_handler = std::function<void(adc_result)>;

  // Exact rates supported by the 1 MHz timer tick, with a 16-bit period.
  bool initialize(uint32_t sample_rate_hz) {
    if (busy_ || sample_rate_hz < 16 || sample_rate_hz > 10000 ||
        1000000 % sample_rate_hz != 0) {
      return false;
    }
    initialized_ = false;
    HAL_NVIC_DisableIRQ(DMA1_Channel1_IRQn);
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_TIM3_FORCE_RESET();
    __HAL_RCC_TIM3_RELEASE_RESET();
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
    uint32_t timer_clock = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != 0) {
      timer_clock *= 2;
    }
    if (timer_clock < 1000000 || timer_clock % 1000000 != 0) {
      return false;
    }
    LL_TIM_SetPrescaler(TIM3, timer_clock / 1000000 - 1);
    LL_TIM_SetAutoReload(TIM3, 1000000 / sample_rate_hz - 1);
    // Load prescaler while ADC triggering is still disabled.
    LL_TIM_GenerateEvent_UPDATE(TIM3);
    LL_TIM_ClearFlag_UPDATE(TIM3);
    LL_TIM_SetTriggerOutput(TIM3, LL_TIM_TRGO_UPDATE);
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_ADC1_FORCE_RESET();
    __HAL_RCC_ADC1_RELEASE_RESET();
    __HAL_RCC_ADC_CONFIG(RCC_ADCPCLK2_DIV6);  // 72 MHz / 6 = 12 MHz.
    pinMode(PA0, INPUT_ANALOG);
    LL_ADC_SetDataAlignment(ADC1, LL_ADC_DATA_ALIGN_RIGHT);
    LL_ADC_SetSequencersScanMode(ADC1, LL_ADC_SEQ_SCAN_DISABLE);
    LL_ADC_REG_SetTriggerSource(ADC1, LL_ADC_REG_TRIG_EXT_TIM3_TRGO);
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
    LL_ADC_ClearFlag_EOS(ADC1);
    HAL_NVIC_ClearPendingIRQ(DMA1_Channel1_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    initialized_ = true;
    return true;
  }

  // Capture at the configured rate; completion fires once after the whole buffer is filled.
  // Buffer must stay alive until completion. Errors call back inline; DMA completion in IRQ.
  void capture_async(std::span<uint16_t> samples, completion_handler callback) {
    if (!initialized_) {
      callback(std::unexpected(adc_error::not_initialized));
      return;
    }
    if (busy_) {
      callback(std::unexpected(adc_error::busy));
      return;
    }
    if (samples.empty() || samples.size() > 65535 || samples.data() == nullptr) {
      callback(std::unexpected(adc_error::invalid_buffer));
      return;
    }
    completion_ = std::move(callback);
    busy_ = true;
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_ClearFlag_GI1(DMA1);
    LL_DMA_ConfigTransfer(DMA1, LL_DMA_CHANNEL_1,
        LL_DMA_DIRECTION_PERIPH_TO_MEMORY | LL_DMA_MODE_NORMAL |
        LL_DMA_PERIPH_NOINCREMENT | LL_DMA_MEMORY_INCREMENT |
        LL_DMA_PDATAALIGN_HALFWORD | LL_DMA_MDATAALIGN_HALFWORD | LL_DMA_PRIORITY_HIGH);
    LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_1,
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&ADC1->DR)),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(samples.data())),
        LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, samples.size());
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);
    LL_ADC_ClearFlag_EOS(ADC1);
    LL_ADC_REG_SetDMATransfer(ADC1, LL_ADC_REG_DMA_TRANSFER_UNLIMITED);
    LL_TIM_SetCounter(TIM3, 0);
    LL_TIM_ClearFlag_UPDATE(TIM3);
    LL_ADC_REG_StartConversionExtTrig(ADC1, LL_ADC_REG_TRIG_EXT_RISING);
    // First sample after one timer period; capture duration is samples.size() / sample_rate_hz.
    LL_TIM_EnableCounter(TIM3);
  }

  void handle_interrupt() {
    const bool failed = LL_DMA_IsActiveFlag_TE1(DMA1);
    if (!busy_ || (!failed && !LL_DMA_IsActiveFlag_TC1(DMA1))) {
      return;
    }
    LL_TIM_DisableCounter(TIM3);
    LL_ADC_REG_StopConversionExtTrig(ADC1);
    LL_ADC_REG_SetDMATransfer(ADC1, LL_ADC_REG_DMA_TRANSFER_NONE);
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_ClearFlag_GI1(DMA1);
    busy_ = false;
    // Retain the handler: destroying it here could free heap memory in the ISR.
    completion_(failed ? adc_result(std::unexpected(adc_error::dma_transfer)) : adc_result{});
  }

 private:
  completion_handler completion_;
  volatile bool busy_ = false;
  bool initialized_ = false;
};

inline adc_driver adc;

// Define in exactly one translation unit to emit the strong vector-table handler.
#ifdef ADC_DRIVER_IMPLEMENTATION
extern "C" void DMA1_Channel1_IRQHandler() {
  adc.handle_interrupt();
}
#endif
