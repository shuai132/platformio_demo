#include "log.h"

#include <Arduino.h>

#ifdef MAIN_FREERTOS

#include "STM32FreeRTOS.h"

uint32_t get_tid() {
  return (uint32_t)xTaskGetCurrentTaskHandle();
}

#endif

/*
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

int _write(int fd, char *buffer, int size) {
  return DEBUG_SERIAL.write(buffer, size);
}

#ifdef __cplusplus
}
#endif
*/
