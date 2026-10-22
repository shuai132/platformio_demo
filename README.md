# platformio_demo

`src/main.cpp` demonstrates concurrent LED blinking and serial heartbeats
with [coro](https://github.com/shuai132/coro). All environments use this C++20 demo.

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

`src/main.cpp` selects the platform driver and starts the application
coroutines. One shared scheduling queue serves the two existing platform
drivers, keeping scheduling policy separate from platform wake-up APIs.

| Component | Responsibility | Dependencies |
| --- | --- | --- |
| `timed_executor` | Common coroutine queue, delayed dispatch, absolute deadlines and stop state | `coro`, standard C++ |
| `esp32_executor` | Drive the queue with one-shot `esp_timer` callbacks and a 64-bit microsecond clock | `timed_executor`, ESP-IDF |
| `arduino_executor` | Drive the queue through `poll()` and `sleep_if_idle()`; extend the wrapping `micros()` clock | `timed_executor`, Arduino |

The application components depend on `timed_executor`, so they use the same
code with either platform driver. The common queue has no platform macros or
GPIO logic. Drivers decide when to run callbacks; application coroutines decide
what work to schedule.

On ESP32, `setup()` queues all coroutines and then calls `executor.start()` as
its final executor operation. This transfers queue ownership to the
high-priority `ESP_TIMER_TASK` task. Each timer wake runs up to 32 ready
callbacks, then rearms for the nearest deadline. Time comes directly from
`esp_timer_get_time()`. The driver exposes no polling API. `loop()` simply
uses `delay(1000)`; coroutine scheduling continues independently of the loop
task, including Arduino's automatic two-second yield on single-core ESP32.

On ESP8266, the loop task owns the executor and calls `poll()` followed by
`sleep_if_idle()`. Idle waits reserve 1 ms before the next deadline; shorter
waits return for continued polling. Longer waits use Arduino `delay()`.

Both drivers require a single owner. After ESP32 `start()`, access the
executor and coroutine state only from executor callbacks; do not
post from `loop()`, another task or an ISR. Callbacks must be short and
nonblocking because ESP timer callbacks share one task. The callback budget
bounds the number of callbacks per wake, not the duration of user code.
Keep the executor and detached coroutines alive for the timer's lifetime.
`stop()` stops dispatch; it does not destroy suspended coroutines or support
restarting the executor.

`src/esp8266_atomic.cpp` supplies the two missing 32-bit libatomic operations
used by `coro::delay()` on ESP8266, using interrupt exclusion. GCC 10.3 still
has upstream coroutine regression failures; a successful build of this demo
does not establish full library compatibility.

```sh
git submodule update --init --recursive
pio run -e esp32-s3-arduino
```
