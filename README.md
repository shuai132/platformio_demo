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
不会阻塞主循环。`poll()` 和 `sleep_if_idle()` 在主循环调用；
中断中可通过 `dispatch()` / `post()` 将工作交给主循环。

### 空闲休眠

`include/stm32_arduino_executor.hpp` 提供 STM32 Arduino 平台执行器，
使用 CMSIS 中断控制和普通 Sleep，目前仅针对 STM32F103/Cortex-M3 完成构建验证，
尚未完成上板休眠唤醒验证。它不是跨所有 Arduino 架构的通用实现；
以后支持其他平台时，再提取共用调度逻辑与平台适配层。

`loop()` 在 `executor.poll()` 后调用 `executor.sleep_if_idle()`。
没有待处理的中断回调、也没有到期的定时任务时，执行器通过 `WFI` 进入普通 Sleep。
未来才到期的任务不会阻止休眠；SysTick、DMA 或其他已启用的中断唤醒 CPU 后继续轮询。
检查队列和进入休眠之间屏蔽中断，避免中断刚提交工作、主循环却继续入睡的竞争。

此实现保留 SysTick 和 `millis()` 时基，通常每 1 ms 唤醒检查一次；
不使用 Stop 或 Standby，也不配置具体外设的时钟或中断，这些由平台和驱动负责。
执行器不依赖特定外设；本示例中的 TIM3、ADC 和 DMA 在 Sleep 中继续工作。
仅在主线程且中断未被屏蔽时允许休眠，执行器停止后直接返回。
调用方应先处理其他主循环工作再调用此方法；它只检查本执行器的队列。
实际省电效果需要在开发板上测量，板载 LED、调试器和外设也会消耗电流。

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
执行器将中断中的协程恢复请求交给主循环，`loop()` 调用 `executor.poll()` 处理任务，
随后调用 `executor.sleep_if_idle()` 在空闲时休眠。
采样期间 blink 和心跳继续运行，不需要 ADC 轮询。

本例独占 ADC1 的 PA0 通道、TIM3、DMA1 通道 1 和 `DMA1_Channel1_IRQHandler`。
不要同时使用这些外设或调用 `analogRead()`。缓冲区、驱动和等待中的协程必须保持存活直到完成。
初始化校准会短暂同步等待；整批采样没有超时或取消机制。
执行器使用固定容量八个任务的中断邮箱，邮箱满时终止执行，避免静默丢弃恢复请求。
PA0 输入范围为 0–3.3 V，悬空读数没有意义。本例用于演示异步等待；100 Hz 不适合音乐频谱。
