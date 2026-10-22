#include <Arduino.h>

#include <coro/coro.hpp>

#if defined(ARDUINO_ARCH_ESP32)
#include "esp32_executor.hpp"
#else
#include "arduino_executor.hpp"
#endif
#include "breathing_led.hpp"
#include "software_pwm.hpp"

namespace {

// ESP32 transfers executor ownership to the esp_timer task after setup.
#if defined(ARDUINO_ARCH_ESP32)
esp32_executor executor;
#else
arduino_executor executor;
#endif

using led_pwm = software_pwm<1000>;
led_pwm pwm_channels[] = {{executor, 4}, {executor, 5}, {executor, 6}};
// PWM duty in per mille: 300 limits the peak to 30% duty.
constexpr uint16_t max_led_duty = 300;
breathing_led<led_pwm> leds[] = {
    {executor, pwm_channels[0], 3000, 0, max_led_duty},
    {executor, pwm_channels[1], 3000, 1000, max_led_duty},
    {executor, pwm_channels[2], 3000, 2000, max_led_duty},
};

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println("coro start");

  for (auto& pwm : pwm_channels) {
    coro::spawn(executor, pwm.run());
  }
  for (auto& led : leds) {
    coro::spawn(executor, led.run());
  }
#if defined(ARDUINO_ARCH_ESP32)
  executor.start();
#endif
}

void loop() {
#if defined(ARDUINO_ARCH_ESP32)
  // PWM coroutines keep running even while this task is blocked.
  delay(1000);
#else
  executor.poll();
  executor.sleep_if_idle();
#endif
}
