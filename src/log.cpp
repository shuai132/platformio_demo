#include "log.h"

#ifdef MAIN_FREERTOS

#include "STM32FreeRTOS.h"

uint32_t get_tid() {
  return (uint32_t)xTaskGetCurrentTaskHandle();
}

#endif
