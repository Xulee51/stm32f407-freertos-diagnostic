# FreeRTOS Robot Diagnostic Terminal

这是全新建立的 STM32F407ZGT6 工程，不继承旧日历工程的业务代码。

当前已验证开发基线（节点 0–8）：

- 8 MHz HSE -> 168 MHz SYSCLK；
- USART1 PA9/PA10，115200-8-N-1；
- FreeRTOS 1 kHz Tick；
- `health`、`logger`、`input`、`can`、`ui` 五个任务；
- Queue、Event Group、任务通知、Heap/Stack 异常钩子；
- IWDG、任务心跳监督和编译期故障注入；
- ILI9806/FSMC LCD、CST716 软件 I2C + EXTI 触摸输入；
- CAN1 500 kbit/s 静默内部回环；
- HAL、CMSIS、FreeRTOS 源码来自 ST 官方 STM32CubeF4 仓库。

2026-08-25 的 CAN1 实机回归连续运行 45 秒，稳定结果为：

```text
health bits=0x1f ... qdrop=0 edrop=0 ... can=45/45 irq=45 cerr=0 ... supervisor=ok wd=ok
```

这证明当前 MCU 内部 CAN 控制器、FIFO、中断、任务通知和日志链路可用；不等于 TJA1040、CANH/CANL、ACK、Bus-Off 或外部 CAN 节点通信已经通过。

项目节点、分支集成状态和下一步计划见 [`PROJECT_STATUS.md`](PROJECT_STATUS.md)。

## Windows 开发环境

本项目规定使用 **Git Bash** 作为日常命令终端，不需要安装 MSYS2。

当前工具链：

- CMake 4.2.1；
- Ninja 1.13.0；
- STM32CubeIDE 1.19.0 内置 ARM GCC 13.3.1；
- STM32CubeProgrammer 2.20.0；
- ST-LINK。

首次安装 CMake 与 Ninja：

```bash
python.exe -m pip install --user cmake==4.2.1 ninja==1.13.0
```

每次新打开 Git Bash 后，进入项目并加载工具环境：

```bash
cd /c/Users/xlc44/Desktop/STM32F407_GitHub_References/freertos_robot_diagnostic
source tools/env.sh
```

配置并构建 Debug 固件：

```bash
cmake --preset debug
cmake --build --preset debug
```

### 命令参数说明

以上命令使用的是 Bash 语法，代码块语言应理解为 `bash`，不是 Lua。

```bash
cmake --preset debug -DDIAG_FAULT_MODE=queue
```

- `cmake`：调用 CMake 配置工具；
- `--preset debug`：使用 `CMakePresets.json` 中名为 `debug` 的配置，选择 Ninja、ARM GCC 和 `build/debug` 输出目录；
- `-D变量=值`：在配置阶段设置一个 CMake 缓存变量；这里把 `DIAG_FAULT_MODE` 设置为 `queue`，生成队列故障注入固件；
- `DIAG_FAULT_MODE=none`：正常固件；
- `DIAG_FAULT_MODE=queue`：约第 5 秒快速填充日志队列，验证 `qdrop`；
- `DIAG_FAULT_MODE=watchdog`：约第 5 秒停止刷新 IWDG，验证自动复位。
- `DIAG_FAULT_MODE=logger`：约第 5 秒让 logger 停止更新心跳，验证 supervisor 主动停止刷新 IWDG。

```bash
cmake --build --preset debug
```

- `--build`：让 CMake 调用已配置好的构建工具；
- `--preset debug`：使用与 `debug` 配置对应的构建目录；
- 这一步只编译和链接，不会烧录 MCU。

```bash
cmake --build --preset debug --target flash
```

- `--target flash`：执行 CMake 中定义的 `flash` 目标；
- 该目标先确保固件构建成功，再调用 STM32CubeProgrammer 通过 ST-LINK 烧录并复位；
- 不带 `--target flash` 时，默认目标只是生成 ELF/BIN，不会改变板上程序。

`-D` 设置会保存在 `build/debug/CMakeCache.txt` 中，因此切换测试模式后必须明确恢复：

```bash
cmake --preset debug -DDIAG_FAULT_MODE=none
cmake --build --preset debug
```

后续增量编译只需要：

```bash
cmake --build --preset debug
```

## 当前软件结构

| 任务 | 优先级 | 职责 |
| --- | ---: | --- |
| `health_task` | 4 | 汇总健康位、Heap/Stack、丢包、触摸/CAN 计数，监督任务心跳并刷新 IWDG |
| `logger_task` | 3 | 独占 USART1 发送路径，输出队列中的日志 |
| `input_task` | 2 | 等待 CST716 EXTI 通知并保留周期轮询，产生 UI 事件 |
| `can_task` | 2 | 每秒执行一次 CAN1 静默内部回环并处理 RX FIFO0 |
| `ui_task` | 1 | 独占 ILI9806/FSMC，消费 UI 事件并刷新状态页 |

`log_queue` 用于统一串口输出，`ui_event_queue` 用于隔离触摸采集和 LCD 刷新，ISR 只做 HAL 中断处理和任务通知，不在中断上下文执行软件 I2C、LCD 绘图或日志格式化。

## FreeRTOS 诊断指标与故障注入

正常固件每秒输出当前 Heap、历史最小 Heap、日志/UI 队列丢包数、触摸/CAN 中断与收发计数、五个任务的栈高水位（单位：word）、监督器和看门狗状态：

```text
[      1000] health bits=0x1f heap=... min_heap=... qdrop=0 edrop=0 tirq=... can=.../... irq=... cerr=0 stack_hwm=.../.../.../.../... supervisor=ok wd=ok
```

IWDG 使用 LSI，约 3 秒超时，由 `health_task` 每秒刷新。启动日志会报告上一次是否由 IWDG 复位：

```text
[BOOT] reset cause=IWDG
```

故障注入只用于测试固件，不要把测试配置烧录成长期运行版本：

```bash
# 队列满测试：约第 5 秒产生 qdrop > 0
cmake --preset debug -DDIAG_FAULT_MODE=queue
cmake --build --preset debug

# 看门狗测试：约第 5 秒停止刷新，约 3 秒后复位；重启日志应显示 reset cause=IWDG
cmake --preset debug -DDIAG_FAULT_MODE=watchdog
cmake --build --preset debug

# 任务监督测试：让 logger 卡死，supervisor 应发现并停止刷新 IWDG
cmake --preset debug -DDIAG_FAULT_MODE=logger
cmake --build --preset debug

# 恢复正常固件配置
cmake --preset debug -DDIAG_FAULT_MODE=none
cmake --build --preset debug
```

检查 ST-LINK：

```bash
cmake --build --preset debug --target probe
```

烧录（会覆盖 MCU 当前固件）：

```bash
cmake --build --preset debug --target flash
```

固件输出位于 `build/debug/`：

- `freertos_robot_diagnostic.elf`；
- `freertos_robot_diagnostic.bin`；
- `freertos_robot_diagnostic.map`；
- `compile_commands.json`。

## 旧构建入口

迁移验收期间暂时保留 Makefile 和旧脚本作为内部回退入口，但文档和日常调试只提供 Git Bash + CMake 命令。

## 文档索引

- [`PROJECT_STATUS.md`](PROJECT_STATUS.md)：节点 0–9、当前分支集成状态和节点 10 UART CLI 计划；
- [`docs/board_pin_map.md`](docs/board_pin_map.md)：板级引脚与资源合同；
- [`docs/lcd_bringup.md`](docs/lcd_bringup.md)：ILI9806/FSMC 最小验收；
- [`docs/touch_cst716_bringup.md`](docs/touch_cst716_bringup.md)：CST716 轮询与 EXTI 验收；
- [`docs/ui_event_lcd_bringup.md`](docs/ui_event_lcd_bringup.md)：UI 事件队列和 LCD 状态页；
- [`docs/can_bringup.md`](docs/can_bringup.md)：CAN 板级合同与内部回环证据；
- [`docs/学习笔记.md`](docs/学习笔记.md)：问题、原理、代码位置和实机证据。

Ethernet/LAN8720A、W25Q128、RS485、电机、ESP8266 和外部 CAN 总线尚未集成。每个新模块都应继续采用“板级合同 -> 最小驱动 -> 可观察指标 -> 故障注入/实机验收”的增量方式加入。
