#pragma once

#include <stdint.h>

struct LEDState {
  uint8_t led1;
  uint8_t led2;
  uint8_t led3;
  uint8_t led4;
};

struct AllState {
  struct LEDState ledState;
  uint8_t ledBootOn;
  uint8_t ledStateOn;
};
