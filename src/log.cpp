#include "log.h"

#include <Arduino.h>

#ifdef MAIN_FREERTOS

#include "STM32FreeRTOS.h"

uint32_t get_tid() {
  return (uint32_t)xTaskGetCurrentTaskHandle();
}

#endif

std::string get_time() {
  uint32_t ms = getCurrentMillis();
  uint32_t hours = ms / (1000 * 60 * 60);
  ms %= (1000 * 60 * 60);
  uint32_t minutes = ms / (1000 * 60);
  ms %= (1000 * 60);
  uint32_t seconds = ms / 1000;
  ms %= 1000;
  char timeStr[13];
  snprintf(timeStr, sizeof(timeStr), "%02lu:%02lu:%02lu.%03lu", hours, minutes, seconds, ms);
  return {timeStr};
}

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
