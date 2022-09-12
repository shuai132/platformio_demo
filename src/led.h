#pragma once

#include "rpc_msg.h"

void ledSetState(LEDState ledState);
LEDState ledGetState();

void ledInit();
void ledOn();
void ledOff();
