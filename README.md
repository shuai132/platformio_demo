# platformio_demo

## 协程 blink 示例

初始化 `lib/coro` 子模块：

```sh
git submodule update --init --recursive
```

编译或通过 ST-Link 烧录：

```sh
pio run -e stm32_arduino_coro
pio run -e stm32_arduino_coro -t upload
```

`src/main_coro.cpp` 使用 C++20 协程在 PC13 上每 100 ms 切换一次 LED。
心跳协程每秒通过 115200 波特率串口打印运行时间，两个协程共用执行器。
`co_await coro::delay(100)` 挂起协程，由 `loop()` 中轮询的执行器在时间到达后恢复，
不会阻塞主循环。执行器仅用于主循环，不能在中断中调用。

## 等待 ADC 结果

`sample_adc()` 每隔 500 ms 在 PA0（ADC1 通道 0）启动一次 12 位采样：

```cpp
auto value = co_await adc.read();
if (value) {
  Serial.println(*value);  // 0..4095
}
```

`include/async_adc.hpp` 和 `src/async_adc.cpp` 使用 `coro::callback_awaiter`：
ADC 中断只保存结果，`loop()` 中的 `adc.poll()` 调用完成回调，恢复协程。
等待采样时 blink 和心跳继续运行；直接调用阻塞的 `analogRead()` 仍会阻塞主循环。

本例独占 ADC1 和 `ADC1_2_IRQHandler`，不要同时使用 `analogRead()` 或其他 ADC1 驱动。
一次只支持一个挂起的请求；未初始化、忙或转换超过 10 ms 时返回 `std::nullopt`。
初始化校准会短暂同步等待；超时后需要重新 `begin()`，示例已处理。
等待中的协程必须保持存活直到完成，不支持主动取消。
PA0 可接 GND、3.3 V 或电位器滑动端（两端接 GND 和 3.3 V），输入范围为 0–3.3 V。
悬空引脚的读数没有意义。
