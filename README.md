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

All environments use the single-threaded Arduino executor.
It uses `millis()` timers with millisecond resolution and
runs one ready callback per `loop()` iteration. `sleep_if_idle()` waits only when
no callback is ready, for up to 10 ms or the next timer, whichever comes first.
It uses Arduino `delay()` to yield execution, rather than hardware deep sleep.
Use the executor only from the loop task,
never from another task or ISR. `stop()` stops polling; it does not cancel
suspended coroutines.

`src/esp8266_atomic.cpp` supplies the two missing 32-bit libatomic operations
used by `coro::delay()` on ESP8266, using interrupt exclusion. GCC 10.3 still
has upstream coroutine regression failures; a successful build of this demo
does not establish full library compatibility.

```sh
git submodule update --init --recursive
pio run -e esp32-s3-arduino
```
