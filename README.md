# platformio_demo

Three coroutine-driven software PWM channels on GPIO 3, 4 and 5, using
[coro](https://github.com/shuai132/coro). Each carrier runs at 1000 Hz. The
breathing periods are 2, 3 and 4.5 seconds; peak duty is 30%. Each LED fades
all the way to off. Connect active-high LEDs through current-limiting
resistors to the GPIOs, with their cathodes connected to GND. Check the
pinout of the actual board before wiring.

## Components

| File | Responsibility |
| --- | --- |
| `include/software_pwm.hpp` | Pin, frequency, duty, PWM edges and stop/restart |
| `include/breathing_led.hpp` | Brightness curve and breathing period |
| `include/cooperative_executor.hpp` | Single-owner coroutine scheduling with absolute deadlines |
| `include/platform_io.hpp` | Select the platform adapter |
| `include/arduino_platform.hpp` | Arduino clock, GPIO and console adapter |
| `src/main.cpp` | Construct components and start platform execution |
| `include/esp32_executor_runner.hpp` | Run Arduino ESP32 coroutines from `ESP_TIMER_TASK` |
| `baremetal/esp32c3/` | C3 startup, RAM linker layout, single-thread runtime and image builder |
| `include/demo_config.hpp` | Pins, PWM frequencies, phase offsets and breathing configuration |

The PWM API is separate from the breathing animation:

```cpp
software_pwm pwm(3, 1000);              // GPIO, frequency in Hz (1–1000)
breathing_led led(pwm, 2000, 300);      // PWM, period in ms, peak duty per mille
if (pwm.begin()) {
  coro::spawn(executor, pwm.run(executor));
  coro::spawn(executor, led.run(executor));
}
```

`set_duty(0..1000)` accepts per-mille duty; `set_duty_ppm(0..1000000)`
preserves finer steps near darkness. Values above the range are clamped.
`breathing_led` updates every 1 ms, computes a quadratic curve with Q16
brightness precision, and applies its configurable peak through
`set_max_brightness()`. The animation owns brightness changes while running;
manual PWM duty changes may be overwritten by its next update.

`software_pwm::stop()` immediately drives the pin low. Its suspended `run()`
coroutine exits on its next scheduled wake; `is_running()` remains true until
that happens. To restart, wait for it to become false, call `begin()`, set
new duty and spawn `run()` again. A submitted coroutine that has not started
also requires the PWM object to remain alive. Keep the executor, PWM and
animation objects alive for all their submitted coroutines. The demo uses
static objects. The breathing coroutine runs continuously; stopping its PWM
does not destroy the animation coroutine.

The optional second `run(executor, phase_us)` argument offsets the carrier.
The demo uses offsets 0, 333 and 666 us. At its 30% peak the three pulse
windows do not overlap under normal scheduling. All operations belong to
one executor owner; do not manipulate the PWM or queue concurrently from
another task or an ISR. `cooperative_executor::stop()` stops dispatch only,
without cancelling or destroying suspended coroutines. Poll the extended
microsecond clock at least once per 32-bit clock wrap (about 71 minutes).

## C3 Arduino / bare-metal comparison

Both C3 environments compile the same `src/main.cpp`, `software_pwm.hpp`,
`breathing_led.hpp`, `cooperative_executor.hpp` and `demo_config.hpp`.
There is one implementation and one configuration, with no copied algorithm
headers under `baremetal/`. Both C3 targets also use `esp32c3_io.hpp` for the
same GPIO register writes, 16 MHz system-timer clock and CPU cycle counter.

| Setting | Both C3 environments |
| --- | --- |
| GPIO / PWM frequency | 3/4/5, each 1000 Hz |
| Duty / brightness curve | 0–30%, Q16 quadratic curve, parts-per-million duty |
| Breathing periods / update | 2/3/4.5 seconds, updates every 1 ms |
| Carrier phase offsets | 0/333/666 us |
| Pulse scheduling | Direct cycle wait for short pulses; 16 us falling-edge guard for longer pulses |
| CPU frequency | Fixed 40 MHz |

`esp32-c3-arduino` retains Arduino/FreeRTOS startup and an `ESP_TIMER_TASK`
runner; `esp32-c3-baremetal` uses the custom startup and a polling loop with
interrupts disabled. These framework execution differences can still cause
actual edge timing to differ. Arduino executes from its normal framework
memory layout; bare metal uses its RAM-only layout. Shared algorithms do not
guarantee identical measured pulses under interrupts and task preemption.
Do not enable dynamic CPU frequency changes or reuse the cycle counter for
another purpose during the comparison.

Build and upload either environment without changing Git branches:

```sh
pio run -e esp32-c3-arduino
pio run -e esp32-c3-baremetal
pio run -e esp32-c3-arduino -t upload
pio run -e esp32-c3-baremetal -t upload
```

Both use 115200 baud for startup output. Uploading selects which firmware
runs on the same board; compare with the same LED wiring.

## ESP32-C3 bare metal

`esp32-c3-baremetal` uses no Arduino, ESP-IDF application framework or RTOS.
The ROM bootloader loads a RAM-only image from flash offset `0x0`, then
starts the custom assembly entry. There is no application second-stage
bootloader, partition table or flash MMU setup. C++ constructors run before
`main()`. The program continuously polls the cooperative executor with
interrupts disabled. The CPU runs directly from the 40 MHz crystal; the
runtime calibrates its cycle counter against the system timer at startup.

For short pulses, the C3 adapter uses the cycle counter to wait directly for
the falling edge. Longer pulses suspend until 16 us before their target,
then wait for the remaining cycles. This avoids coroutine dispatch overhead
at the falling edge of very short dark-end pulses. Cycle-counter resolution
is 25 ns at 40 MHz, but GPIO instructions and bus latency still impose a
minimum physical pulse width; this is not a claim of 25 ns output accuracy.

The linker reserves 32 KiB for the heap and 16 KiB for the stack. These
reservations are included in the reported RAM usage. `scripts/verify_baremetal.py`
checks the RAM layout, unresolved symbols, accidental RTOS/thread/exception
linkage and the ROM image's checksum and SHA-256 digest. The runtime uses
single-thread libc/C++ support tied to the pinned GCC 14 toolchain.

```sh
git submodule update --init --recursive
pio run -e esp32-c3-baremetal
pio run -e esp32-c3-baremetal -t upload
pio device monitor -b 115200
```

Flashing this environment replaces the image at flash offset `0x0`. Returning
to an Arduino application requires uploading its normal bootloader,
partition table and firmware. Select pins 0–21 valid for the particular C3
board; the low-level C3 GPIO adapter does not validate pin numbers.

## Other environments

The C3, S3 and original ESP32 Arduino environments retain their framework. Their one-shot
`esp_timer` runner owns the executor after `setup()`, so PWM continues while
Arduino's loop task waits. It waits actively for deadlines within 25 us and
rearms the timer after a 100 us callback budget. User callbacks must be short
and nonblocking. Interrupts and other timer callbacks can still cause jitter.
ESP8266 polls directly from `loop()` without idle delays. The S3 and original ESP32 Arduino adapters
have microsecond pulse resolution; both C3 environments use the shared
cycle-counter pulse implementation. Missed PWM periods are skipped instead of replayed.
No environment uses LEDC or executes coroutines in an ISR.

ESP32 environments pin pioarduino 55.03.312-1 (GCC 14.2); Arduino environments
use Arduino 3.3.12. ESP8266 pins espressif8266 4.2.1. Its macOS GCC 10.3
binaries require Rosetta 2 on Apple Silicon. `src/esp8266_atomic.cpp` supplies
two missing 32-bit atomic operations. A successful demo build does not
establish complete compatibility of GCC 10.3 with the coroutine library.

Physical pulse timing and perceived brightness require verification on the
board with a logic analyzer and the actual LEDs.

## Software PWM experiments

See [the experiment record](docs/PWM_EXPERIMENTS.md) for the dedicated-task,
maximum-priority and interrupt-disabled trials and their observed limitations.
The Arduino target retains normal `esp_timer` dispatch, interrupts and watchdogs.
