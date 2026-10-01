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

`src/main_coro.cpp` 使用 C++23 编译模式，在 PC13 上每 100 ms 切换一次 LED。
心跳协程每秒通过 115200 波特率串口打印运行时间，两个协程共用执行器。
`co_await coro::delay(100)` 挂起协程，由 `loop()` 中轮询的执行器在时间到达后恢复，
不会阻塞主循环。执行器仅用于主循环，不能在中断中调用。

## 等待一批 ADC 数据

示例在 PA0 以 **100 Hz** 采集 **64 点**：每 10 ms 一点，一批约 **640 ms**。
使用 TIM3 定时触发 ADC1，DMA1 通道 1 将数据写入调用方缓冲区，整批完成后回调。
采样循环中的额外 `coro::delay()` 已注释，完成一批后开始下一批。

在 `src/main_coro.cpp` 顶部修改 `ADC_SAMPLE_RATE_HZ` 和 `ADC_SAMPLE_COUNT` 即可调整采样率和点数。
驱动通过 `adc.initialize(ADC_SAMPLE_RATE_HZ)` 接收配置。
支持 16–10000 Hz 内能整除 1000000 的整数采样率，例如 100、1000、8000 Hz；无效配置返回 `false`。

ADC 实现全部在头文件：`adc_driver.hpp` 提供回调 API，`async_adc.hpp` 仅做协程适配。
在一个编译单元中、首次包含驱动前定义 `ADC_DRIVER_IMPLEMENTATION`，生成 DMA 中断入口。
`src/main_coro.cpp` 已配置。

```cpp
std::array<uint16_t, 64> samples{};
adc.initialize(100);  // 100 Hz
adc.capture_async(samples, [](adc_result result) {
  // 完成回调在 DMA 中断中运行，只记录状态，不在这里打印。
});
```

协程适配：

```cpp
std::array<uint16_t, 64> samples{};
auto result = co_await capture_adc(adc, samples);  // 约 640 ms
if (result) {
  Serial.println(samples.front());
}
```

结果类型是 C++23 的 `std::expected<void, adc_error>`，数据存放在 `samples` 中。
失败时 `result.error()` 区分未初始化、忙、无效缓冲区和 DMA 错误。
执行器将中断中的协程恢复请求交给主循环，`loop()` 只需调用 `executor.poll()`。
采样期间 blink 和心跳继续运行，不需要 ADC 轮询。

本例独占 ADC1 的 PA0 通道、TIM3、DMA1 通道 1 和 `DMA1_Channel1_IRQHandler`。
不要同时使用这些外设或调用 `analogRead()`。缓冲区、驱动和等待中的协程必须保持存活直到完成。
初始化校准会短暂同步等待；整批采样没有超时或取消机制。
执行器使用固定容量八个任务的中断邮箱，邮箱满时终止执行，避免静默丢弃恢复请求。
PA0 输入范围为 0–3.3 V，悬空读数没有意义。本例用于演示异步等待；100 Hz 不适合音乐频谱。
