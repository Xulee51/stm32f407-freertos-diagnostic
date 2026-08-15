# FreeRTOS Robot Diagnostic Terminal

这是全新建立的 STM32F407ZGT6 工程，不继承旧日历工程的业务代码。

当前基线：

- 8 MHz HSE -> 168 MHz SYSCLK；
- USART1 PA9/PA10，115200-8-N-1；
- FreeRTOS 1 kHz Tick；
- `health`、`logger`、`ui` 三个最小任务；
- Queue、Event Group、Heap/Stack 异常钩子；
- HAL、CMSIS、FreeRTOS 源码来自 ST 官方 STM32CubeF4 仓库。

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

后续增量编译只需要：

```bash
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

串口预期每秒输出 `health bits` 和 FreeRTOS 剩余 Heap。LCD、触摸、以太网、CAN、RS485 将在基线验收后按阶段加入。

学习过程中的问题与解释记录在 [`docs/学习笔记.md`](docs/学习笔记.md)。
