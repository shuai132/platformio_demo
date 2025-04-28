#include "log.h"

#include "STM32FreeRTOS.h"

uint32_t get_tid() {
  return (uint32_t)xTaskGetCurrentTaskHandle();
}
