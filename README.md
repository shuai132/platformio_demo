# platformio_demo

`src/main.cpp` drives three breathing LEDs on GPIO 4, 5 and 6 using
[coro](https://github.com/shuai132/coro). All environments use this C++20 demo.

- `software_pwm<1000>` owns GPIO output and produces a nominal 1000 Hz
  software PWM carrier (1000 us period). `set_duty()` accepts 0–1000 per mille,
  including fully off/on, and takes effect on the next cycle.
- `breathing_led<Pwm>` changes duty every 10 ms with a quadratic brightness
  curve scaled to a configurable maximum PWM duty (the fifth constructor
  argument, clamped to 0–1000). The demo sets `max_led_duty = 300` in
  `src/main.cpp`, limiting the peak to 30% duty. This scales the whole curve
  instead of clipping the peak into a plateau; duty percentage is not a
  measurement of perceived brightness. Each breath lasts 3 seconds; the three
  instances start at phases 0, 1 and 2 seconds. Set all phase arguments to zero for synchronized breathing.
- `setup()` starts one PWM coroutine and one animation coroutine per LED.
  These objects have static lifetime and run on the same executor.

Connect each active-high LED through a current-limiting resistor to its GPIO,
with its cathode connected to GND. Check the board pinout before wiring:
The GPIO mapping is defined in `src/main.cpp`. The demo also starts Serial at
115200 baud. Choose pins appropriate for your board; GPIO 6 is connected to
flash on many ESP8266 and original ESP32 modules.

The ESP32 environments pin [pioarduino](https://github.com/pioarduino/platform-espressif32)
55.03.312-1 (Arduino 3.3.12, GCC 14.2) for C++20 coroutine support.
The official `espressif32` 6.11.0 Arduino environment uses GCC 8.4,
which cannot build this demo. pioarduino is a community-maintained
ESP32 platform compatible with PlatformIO. The ESP8266 environment pins
the official espressif8266 platform to 4.2.1.

On Apple Silicon, the ESP8266 GCC 10.3 toolchain requires Rosetta 2 because
its macOS binaries are x86_64. A `Bad CPU type in executable` error means
that the host cannot run the installed toolchain.

## Architecture

`src/main.cpp` selects the platform driver and wires together the executor,
three PWM channels and three breathing animations. GPIO pins, phase offsets,
breath duration and maximum duty are configured here.

| Component | Responsibility | Dependencies |
| --- | --- | --- |
| `timed_executor` | Common coroutine queue, delayed dispatch, absolute deadlines and stop state | `coro`, standard C++ |
| `esp32_executor` | Drive the queue with one-shot `esp_timer` callbacks and a 64-bit microsecond clock | `timed_executor`, ESP-IDF |
| `arduino_executor` | Drive the queue through `poll()` and `sleep_if_idle()`; extend the wrapping `micros()` clock | `timed_executor`, Arduino |
| `software_pwm<Hz>` | Schedule GPIO edges, latch duty per period and skip missed edges | `timed_executor`, Arduino GPIO |
| `breathing_led<Pwm>` | Convert breath phase into a corrected, limited PWM duty | `timed_executor`, PWM duty interface |

The application components depend on `timed_executor`, so they use the same
code with either platform driver. The common queue has no platform macros or
GPIO logic. Drivers decide when to run callbacks; PWM and animation decide
what work to schedule.

On ESP32, `setup()` queues all coroutines and then calls `executor.start()` as
its final executor operation. This transfers queue ownership to the
high-priority `ESP_TIMER_TASK` task. Each timer wake runs up to 32 ready
callbacks, then rearms for the nearest deadline. Time comes directly from
`esp_timer_get_time()`. The driver exposes no polling API. `loop()` simply
uses `delay(1000)`, and software PWM continues during Arduino's automatic
two-second loop-task yield on single-core ESP32.

On ESP8266, the loop task owns the executor and calls `poll()` followed by
`sleep_if_idle()`. Idle waits reserve 1 ms before the next deadline; shorter
waits return for continued polling. Longer waits use Arduino `delay()`.

Both drivers require a single owner. After ESP32 `start()`, access the
executor, PWM duty and coroutine state only from executor callbacks; do not
post from `loop()`, another task or an ISR. Callbacks must be short and
nonblocking because ESP timer callbacks share one task. The callback budget
bounds the number of callbacks per wake, not the duration of user code.
Keep the executor and detached coroutines alive for the timer's lifetime.
`stop()` stops dispatch; it does not destroy suspended coroutines or support
restarting the executor.

PWM and animation use absolute deadlines to avoid cumulative drift; late PWM
resumes skip missed edges. This remains coro software PWM, with no LEDC and no
coroutine execution in an ISR. Interrupts, higher-priority work, shared timer
callbacks and timer dispatch overhead can still cause jitter, especially for
very short pulses near 0%/100% duty. The nominal 1000 Hz carrier is not a hard
real-time guarantee; measure GPIO edges with a logic analyzer for hardware QA.

`src/esp8266_atomic.cpp` supplies the two missing 32-bit libatomic operations
used by `coro::delay()` on ESP8266, using interrupt exclusion. GCC 10.3 still
has upstream coroutine regression failures; a successful build of this demo
does not establish full library compatibility.

```sh
git submodule update --init --recursive
pio run -e esp32-s3-arduino
```
