# 项目状态与路线图

更新时间：2026-08-26

## 当前开发检查点

| 项目 | 状态 |
| --- | --- |
| 当前开发分支 | `feature/can-loopback` |
| 节点 8 提交 | `a7339d5`（本地与 `origin/feature/can-loopback` 一致） |
| 集成基线 | STM32F407ZGT6，168 MHz，FreeRTOS，USART1，IWDG，ILI9806，CST716，CAN1 静默内部回环 |
| 实机健康位 | `0x1f`：boot/logger/ui/input/CAN 均已就绪 |
| CAN 回环证据 | 连续 45 秒：`can=45/45`、`irq=45`、`cerr=0` |
| 系统证据 | `qdrop=0`、`edrop=0`、`supervisor=ok`、`wd=ok` |

`develop` 当前仍在 `834557f`，尚未包含 LCD、触摸、UI 事件层和 CAN 增量。节点 9 先在当前功能分支建立一致的文档检查点；是否合入 `develop` 由代码复核和用户手动 commit/push 后决定。

仓库中的 `local/` 用于本地硬件资料，不属于固件交付内容，本节点不修改也不纳入版本控制。

## 节点进度

| 节点 | 内容 | 状态与证据 |
| ---: | --- | --- |
| 0 | 合并 FreeRTOS 资源监控、任务监督和 IWDG 基线 | 完成；`develop` 检查点 `834557f` |
| 1 | 建立 STM32F407 板级引脚与资源合同 | 完成；见 `docs/board_pin_map.md` |
| 2 | ILI9806/FSMC 最小驱动 | 完成；启动图案和持续日志链路已建立 |
| 3 | CST716 软件 I2C 轮询 | 完成；设备、坐标和按下/释放路径已建立 |
| 4 | LCD 与触摸实机联合验收 | 完成；保留方向、背光和版本字段的板级实测结论 |
| 5 | CST716 PB1 EXTI 唤醒 | 完成；ISR 只清中断并通知任务，50 ms 轮询作为回退 |
| 6 | FreeRTOS UI 事件层和 LCD 状态页 | 完成；`input_task -> ui_event_queue -> ui_task`，`edrop=0` |
| 7 | CAN1 原理图与跳帽合同 | 完成；PA11/PA12、P9、TJA1040、P7、R18 已记录 |
| 8 | CAN1 静默内部回环 | 完成；`health bits=0x1f`、45/45 帧、45 次 IRQ、0 错误 |
| 9 | 整合当前开发版本和项目文档 | 完成，待用户复核并手动提交；README 与本文件形成当前交付入口 |
| 10 | UART CLI | 已规划，尚未实现 |

节点 8 的边界必须保留：内部回环只验证 MCU 内部 bxCAN 和软件链路，外部 TJA1040、CANH/CANL、ACK、错误状态和 Bus-Off 恢复仍是后续独立节点。

## 节点 10：UART CLI 计划

### 目标与边界

在现有 USART1 `115200-8-N-1` 日志口上增加一个小型、可测试的只读命令行，使运行中的固件可以按需查询状态。第一版不允许通过 CLI 烧录、复位、切换 CAN 工作模式、修改 Flash 或触发故障注入，避免把交互功能和高风险控制混在同一节点。

### 设计

```text
USART1 RX IRQ
    -> 固定长度 RX StreamBuffer（FromISR 写入）
    -> cli_task（逐字节组行、解析、分发）
    -> log_queue
    -> logger_task
    -> USART1 TX
```

- RX ISR 只读取字节、写入 StreamBuffer 并通知调度，不做解析、格式化或发送；
- `cli_task` 使用固定缓冲区，不使用动态字符串分配；建议命令行最长 96 字节、最多 6 个参数；
- 支持 `CR`、`LF`、`CRLF`、退格和超长行丢弃恢复；
- CLI 回复继续走 `log_queue`，USART1 TX 仍由 `logger_task` 独占；
- 状态查询先复制一份快照再格式化，避免输出过程中读取到互相矛盾的计数；
- 新增 `EVT_CLI_OK` 和 `cli_heartbeat`，由 supervisor 监督；节点 10 正常健康位预期从 `0x1f` 变为 `0x3f`；
- RX 中断优先级遵守 FreeRTOS `FromISR` API 约束，并记录 RX 溢出、命令行溢出和未知命令计数。

### 第一版命令

| 命令 | 输出 |
| --- | --- |
| `help` | 命令列表和参数格式 |
| `status` | 健康位、Heap、队列丢包、supervisor、IWDG 状态 |
| `tasks` | 六个应用任务的优先级、栈高水位和心跳摘要 |
| `can` | 模式、波特率、TX/RX、IRQ、错误计数和“仅内部回环”边界 |
| `touch` | 触摸初始化状态、最近坐标、事件数、EXTI 和丢包计数 |
| `version` | 固件名称、构建类型和可追踪的版本标识 |

未知命令统一返回短错误和 `help` 提示。第一版每条命令最多产生一条有界回复；若内容过长，应拆分为固定上限的多条消息并显式统计回复丢弃。

### 实施顺序

1. 把 USART1 的 RX GPIO/NVIC/IRQ 入口接通，先用计数器证明每个字符只接收一次；
2. 加入静态 StreamBuffer 和 `cli_task`，验证 CR/LF、退格、空行和超长行恢复；
3. 建立命令表和纯解析函数，让解析逻辑可在主机侧做单元测试；
4. 实现只读状态快照与六条命令，所有回复接入现有 `log_queue`；
5. 把 CLI 就绪位、心跳和错误计数纳入 health/supervisor/LCD 状态；
6. 进行构建、串口交互、压力和回归验收，再更新 README 与学习笔记。

### 验收标准

- 上电后原有周期日志、LCD、触摸、CAN 回环和 IWDG 行为不退化；
- `help/status/tasks/can/touch/version` 均有确定且有界的输出；
- `CR`、`LF`、`CRLF`、退格、空命令、未知命令和超过 96 字节的输入都能恢复到下一条命令；
- 连续快速输入时系统不 HardFault、不复位，RX/行缓冲溢出可计数，不能静默损坏内存；
- 稳态 `health bits=0x3f`、`supervisor=ok`、`wd=ok`，CAN `TX=RX=IRQ` 持续递增且 `cerr=0`；
- 无交互和正常交互条件下 `qdrop=0`、`edrop=0`，CLI 回复不会绕过 logger 直接争用 USART1 TX；
- 节点 8 的 45 秒 CAN 回环证据可重复，触摸按下/释放和 LCD 状态页仍正常。

## 节点 9 交付检查

节点 9 只更新项目文档，不改变固件和板上程序。已使用 `DIAG_FAULT_MODE=none` 完成 Debug 配置与构建（`ninja: no work to do.`），`git diff --check` 通过；烧录不属于本节点。
