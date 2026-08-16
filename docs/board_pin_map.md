# STM32F407ZGT6 板级信息合同

本文档是 `freertos_robot_diagnostic` 接入 LCD/触摸屏前的板级约束。它把“原理图/旧工程中确认的事实”和“必须在新工程实机验收的项目”分开，避免把旧工程的推断直接当成新工程的验收结果。

## 1. 基本信息

| 项目 | 当前合同 | 证据/状态 |
| --- | --- | --- |
| MCU | STM32F407ZGT6，系统时钟 168 MHz | 当前 FreeRTOS 固件已验证 |
| LCD | ILI9806，FSMC 并口，16 位数据 | 旧工程 `calendar/Core/Src/fsmc.c` 与 LCD 驱动 |
| LCD 存储器区域 | FSMC Bank 4（`NE4`） | 旧工程已使用 |
| LCD 命令/数据选择 | FSMC `A6` | 旧工程驱动使用 `TFTLCD_BASE = 0x6C000000 | 0x7E` |
| 触摸 | CST716，软件模拟 I²C | 旧工程驱动已实机读到寄存器 |
| 串口 | USART1 + CH340，115200 8N1 | 当前 FreeRTOS 基线已验证 |

## 2. ILI9806/FSMC 引脚

| LCD 信号 | STM32F407 引脚 | 说明 |
| --- | --- | --- |
| FSMC A6（命令/数据选择） | PF12 | A6_4 |
| FSMC D0 | PD14 | 16 位数据总线 |
| FSMC D1 | PD15 | 16 位数据总线 |
| FSMC D2 | PD0 | 16 位数据总线 |
| FSMC D3 | PD1 | 16 位数据总线 |
| FSMC D4 | PE7 | 16 位数据总线 |
| FSMC D5 | PE8 | 16 位数据总线 |
| FSMC D6 | PE9 | 16 位数据总线 |
| FSMC D7 | PE10 | 16 位数据总线 |
| FSMC D8 | PE11 | 16 位数据总线 |
| FSMC D9 | PE12 | 16 位数据总线 |
| FSMC D10 | PE13 | 16 位数据总线 |
| FSMC D11 | PE14 | 16 位数据总线 |
| FSMC D12 | PE15 | 16 位数据总线 |
| FSMC D13 | PD8 | 16 位数据总线 |
| FSMC D14 | PD9 | 16 位数据总线 |
| FSMC D15 | PD10 | 16 位数据总线 |
| FSMC NOE/RD | PD4 | 读选通 |
| FSMC NWE/WR | PD5 | 写选通 |
| FSMC NE4/CS | PG12 | 片选 |
| LCD 背光控制 | PB15 | 电平有效性仍需新工程实机确认 |

旧工程的 FSMC 参数可作为初始值，不是最终验收结论：

```text
Bank4，SRAM 类型，16 位，异步，ExtendedMode=ENABLE
普通写：AddressSetupTime=2，DataSetupTime=16
扩展写：AddressSetupTime=4，DataSetupTime=9
```

LCD 命令/数据地址约定：

```c
#define TFTLCD_BASE ((uint32_t)(0x6C000000UL | 0x0000007EUL))
```

其中 `0x6C000000` 是 Bank4 基地址，`0x7E` 来自 A6 命令/数据选择和 16 位总线地址对齐。新驱动应集中定义地址，不要在业务代码中散落裸地址。

## 3. CST716 触摸引脚与地址

| 功能 | STM32F407 引脚 | 当前做法 |
| --- | --- | --- |
| SCL | PB0 | 软件模拟 I²C，GPIO 输出 |
| SDA | PF11 | 软件模拟 I²C，按读写阶段切换方向 |
| INT | PB1 | GPIO 输入；第一版先轮询，后续再评估 EXTI |
| RESET | PC13 | GPIO 输出 |

旧工程使用的 7 位 I²C 地址等价于：

```text
写：0x2A
读：0x2B
```

这里记录的是旧驱动的总线字节值。若改用 STM32 HAL I²C API，应传入左移一位后的设备地址，并在代码注释中明确约定，避免重复左移。

## 4. 当前资源占用与暂不接入项

已占用或必须保留：

- USART1：诊断日志和验收输出；
- FreeRTOS SysTick：调度和监督功能；
- IWDG：故障恢复验收；
- LCD FSMC 引脚：不得复用为普通 GPIO；
- PB0/PF11/PB1/PC13：CST716 触摸接口。

本节点暂不接入：Ethernet/LAN8720A、W25Q128、CAN、电机和 ESP8266。后续接入必须先更新此合同并做引脚冲突检查。

## 5. 节点 1 待实机确认

以下项目不能仅由旧工程代码证明，应在新工程逐项记录串口和照片证据：

1. PB15 的有效电平：输出高/低分别观察背光是否点亮；
2. ILI9806 的实际方向和分辨率：先验证竖屏 `480x800`，再验证横屏 `800x480` 是否符合面板丝印/实际画面；
3. LCD 是否需要独立硬件复位：当前引脚合同中没有确认的 LCD_RST；
4. FSMC 初始时序：先使用旧工程参数，再以纯色填充和英文文字连续运行结果决定是否调整；
5. CST716 第一版采用轮询，确认坐标稳定后再增加 PB1 EXTI；
6. 新工程是否已编译进 `stm32f4xx_hal_sram.c`、启用 `HAL_SRAM_MODULE_ENABLED`，并确认 `HAL_SRAM_MspInit` 被调用。

## 6. 节点 1 通过标准

节点 1 只算“板级合同确认”，不等于 LCD 已经驱动成功。通过条件是：

- 本文档中的引脚与旧工程/原理图一致；
- 当前分支工作区在记录前后可追踪；
- LCD 背光有效电平、方向/分辨率和 FSMC 时序有明确的实机结论；
- 触摸接口的 PB0/PF11/PB1/PC13 与现有 USART1、FreeRTOS、IWDG 无冲突。

完成后，节点 2 才开始加入 LCD 最小驱动：FSMC 初始化 → ILI9806 初始化 → 背光 → 纯色清屏 → 英文测试字串；触摸驱动继续保持独立增量。
