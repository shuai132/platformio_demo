#include <coro/coro.hpp>

#include "breathing_led.hpp"
#include "cooperative_executor.hpp"
#include "demo_config.hpp"
#include "platform_io.hpp"
#include "software_pwm.hpp"
#if defined(ARDUINO_ARCH_ESP32)
#include "esp32_executor_runner.hpp"
#endif

namespace {
cooperative_executor executor;
#if defined(ARDUINO_ARCH_ESP32)
esp32_executor_runner pwm_runner(executor);
#endif
constexpr auto& config = demo_config::leds;
software_pwm pwm_outputs[] = {{config[0].pin, config[0].pwm_hz}, {config[1].pin, config[1].pwm_hz}, {config[2].pin, config[2].pwm_hz}};
breathing_led leds[] = {{pwm_outputs[0], config[0].breathing_ms, config[0].max_brightness},
                        {pwm_outputs[1], config[1].breathing_ms, config[1].max_brightness},
                        {pwm_outputs[2], config[2].breathing_ms, config[2].max_brightness}};
static_assert(sizeof(pwm_outputs) / sizeof(pwm_outputs[0]) == demo_config::led_count);
static_assert(sizeof(leds) / sizeof(leds[0]) == demo_config::led_count);
void configure_outputs() {
  pwm_platform::console_begin();
  for (size_t i = 0; i < demo_config::led_count; ++i) {
    if (!pwm_outputs[i].begin()) {
      pwm_platform::log("Invalid PWM configuration");
      continue;
    }
    coro::spawn(executor, pwm_outputs[i].run(executor, config[i].pwm_phase_us));
    coro::spawn(executor, leds[i].run(executor));
  }
#if defined(ARDUINO_ARCH_ESP32)
  if (!pwm_runner.start()) {
    for (auto& pwm : pwm_outputs) pwm.stop();
    pwm_platform::log("Failed to start PWM executor timer");
  }
#endif
}
}  // namespace
#if defined(DEMO_BAREMETAL_C3)
int main() {
  configure_outputs();
  for (;;) executor.poll();
}
#else
void setup() {
  configure_outputs();
}
void loop() {
#if defined(ARDUINO_ARCH_ESP32)
  delay(1000);
#else
  executor.poll();
#endif
}
#endif
