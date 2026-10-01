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
