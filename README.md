# platformio_demo

A simple Arduino demo that blinks an LED and prints to the serial port.

The ESP32 environments pin the community-maintained
[pioarduino](https://github.com/pioarduino/platform-espressif32) platform to
55.03.312-1 (Arduino-ESP32 3.3.12, ESP-IDF 5.5.5, GCC 14.2).
The ESP8266 environment pins the official espressif8266 platform to 4.2.1.

On Apple Silicon, the ESP8266 GCC 10.3 toolchain requires Rosetta 2 because
its macOS binaries are x86_64.

```sh
pio run -e esp32-s3-arduino
```
