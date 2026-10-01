# 项目规则

## 固件体积记录

- 每次提交前运行 `pio run -e stm32_arduino_coro`，确认构建成功，并将 RAM、Flash 占用同步到 `FIRMWARE_SIZE.txt`。
- 每次提交正文记录 RAM、Flash 字节数及相对上次记录的变化；体积未变也要注明。
- 修改构建环境或增加固件目标时，明确记录对应环境，确保前后数据可比较。
