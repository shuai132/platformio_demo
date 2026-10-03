# platformio_demo

`src/main.cpp` demonstrates concurrent LED blinking and serial heartbeats
with [coro](https://github.com/shuai132/coro). All environments use this C++20 demo.

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
