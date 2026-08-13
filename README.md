# FreeRTOS Robot Diagnostic Terminal

这是全新建立的 STM32F407ZGT6 工程，不继承旧日历工程的业务代码。

当前基线：

- 8 MHz HSE -> 168 MHz SYSCLK；
- USART1 PA9/PA10，115200-8-N-1；
- FreeRTOS 1 kHz Tick；
- `health`、`logger`、`ui` 三个最小任务；
- Queue、Event Group、Heap/Stack 异常钩子；
- HAL、CMSIS、FreeRTOS 源码来自 ST 官方 STM32CubeF4 仓库。

构建：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\dev.ps1 build
```

烧录（需要连接 ST-LINK，且会覆盖 MCU 当前固件）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\dev.ps1 flash
```

串口预期每秒输出 `health bits` 和 FreeRTOS 剩余 Heap。LCD、触摸、以太网、CAN、RS485 将在基线验收后按阶段加入。

学习过程中的问题与解释记录在 [`docs/学习笔记.md`](docs/学习笔记.md)。
