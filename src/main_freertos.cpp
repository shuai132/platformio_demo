#ifdef MAIN_FREERTOS

// Simple demo of three threads
// LED blink thread, print thread, and idle loop
#include <Arduino.h>
#include <STM32FreeRTOS.h>

#include "log.h"

const uint8_t LED_PIN = BLINK_LED;

volatile uint32_t count = 0;

// handle for blink task
TaskHandle_t blink;

//------------------------------------------------------------------------------
// high priority for blinking LED
static void vLEDFlashTask(void *pvParameters) {
  UNUSED(pvParameters);
  pinMode(LED_PIN, OUTPUT);

  // Flash led every 1000 ms.
  for (;;) {
    LOG("LED thread");
    digitalToggle(LED_PIN);
    vTaskDelay((1000L * configTICK_RATE_HZ) / 1000L);
  }
}

//------------------------------------------------------------------------------
static void vPrintTask(void *pvParameters) {
  UNUSED(pvParameters);
  while (1) {
    // Sleep for one second.
    vTaskDelay(configTICK_RATE_HZ);

    // Print unused stack for threads.
    LOG("Unused Stack(LED): %lu, count: %u", uxTaskGetStackHighWaterMark(blink), count);
    LOG("Unused Stack(IDLE): %lu", uxTaskGetStackHighWaterMark(xTaskGetIdleTaskHandle()));
    LOG("Unused Stack(PRINT): %lu", uxTaskGetStackHighWaterMark(nullptr));

    // Zero count.
    count = 0;
  }
}

//------------------------------------------------------------------------------
void setup() {
  Serial1.begin(115200);

  // create blink task
  xTaskCreate(vLEDFlashTask, "Task1", configMINIMAL_STACK_SIZE + 256, NULL, tskIDLE_PRIORITY + 2, &blink);

  // create print task
  xTaskCreate(vPrintTask, "Task2", configMINIMAL_STACK_SIZE + 256, NULL, tskIDLE_PRIORITY + 1, NULL);

  // start FreeRTOS
  vTaskStartScheduler();

  // should never return
  LOG("Die");
  while (1)
    ;
}
//------------------------------------------------------------------------------
// WARNING idle loop has a very small stack (configMINIMAL_STACK_SIZE)
// loop must never block
void loop() {
  while (1) {
    // must insure increment is atomic
    // in case of context switch for print
    noInterrupts();
    count++;
    interrupts();
  }
}

#endif
