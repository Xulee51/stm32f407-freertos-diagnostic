# 节点 7：CAN 板级信息确认

本节点只确认原理图和资源合同，不启用 CAN 外设，也不改变当前可运行固件。

## 原理图证据

从《普中-麒麟 F407 开发板原理图》CAN 区域可确认：

| 功能 | 网络/引脚 | 证据 |
| --- | --- | --- |
| CAN 控制器 | CAN1 | CPU 引脚表和 CAN 区域网络名 |
| CAN1_RX | PA11 | P9 标注 `PA11`，CAN 区域标注 `CAN_RX` |
| CAN1_TX | PA12 | P9 标注 `PA12`，CAN 区域标注 `CAN_TX` |
| 收发器 | U8：TJA1040 | CAN 区域器件标号 |
| 总线接口 | P7：CANH、CANL | CAN 区域连接器标注 |
| 终端电阻 | R18：120 Ω | CANH 与 CANL 之间的电阻标注 |
| 收发器供电 | VCC5 | U8 电源网络标注 |

STM32F407 还存在 CAN1 的其他复用映射（例如 PB8/PB9、PD0/PD1），但本板通过 P9 在 `PA11/PA12 <-> CAN_RX/CAN_TX` 与 `PA11/PA12 <-> USB D-/D+` 之间选择。使用 CAN 时，两只跳帽必须接向 U8/CAN 一侧；不能仅因为 MCU 复用表存在就改用其他映射。

## 与当前工程的冲突检查

- PA11/PA12 当前没有被 USART1、LCD FSMC 或 CST716 占用；
- PA11/PA12 同时是 USB FS D-/D+ 复用脚；P9 接错到 USB 一侧时，CAN_RX 可能保持低电平，导致 `HAL_CAN_Start()` 等待 `INAK` 清零超时；
- CAN1 使用 APB1 外设时钟，当前系统时钟下 PCLK1 为 42 MHz；
- U8 的 `S` 脚在原理图中直接接地，因此默认处于正常模式；仍需实测 VCC5 和 PA11/CAN_RX 的静态电平。

## 节点 7 实机前检查

1. 断电调整 P9：两只跳帽都接向 U8/CAN 一侧，而不是 USB D-/D+ 一侧；
2. 上电后用万用表确认 U8 的 VCC5 和 GND，并确认 PA11/CAN_RX 空闲时为高电平；
3. 确认 P7 的 CANH/CANL 没有接反，R18 的 120 Ω 是否作为本节点终端；
4. 若有 USB-CAN 或第二个 CAN 节点，设置相同波特率并共地；
5. 确认总线两端终端电阻总值符合实际拓扑，单板独立工作不能把“发送成功”当作总线验收；
6. 记录外部节点型号、波特率、供电电压和连接线长度。

没有第二个 CAN 节点时，可以先做 CAN 内部回环和寄存器/中断软件验证，但不能把物理 CANH/CANL 通信标记为通过。

## 节点 8：CAN1 内部回环验收

固件将 CAN1 配置为 `CAN_MODE_SILENT_LOOPBACK`、500 kbit/s，每秒发送一帧标准数据帧（`StdId=0x321`、DLC=4）。RX FIFO0 中断只通过 HAL 回调通知 `can_task`，实际读取 FIFO 和日志格式化均在任务上下文完成。

首次测试中 `HAL_CAN_Start()` 返回 `HAL_CAN_ERROR_TIMEOUT`，寄存器表现为 `MCR.INRQ=0`、`MSR.INAK=1`。原理图复核确认 P9 是 CAN/USB 复用选择器；调整 P9 到 CAN 侧并给 PA11 配置弱上拉后，控制器能够退出初始化并完成回环。

2026-08-25 实机连续运行 45 秒，验收证据为：

```text
[CAN] CAN1 internal-loopback init OK
CAN loopback rx id=0x321 dlc=4 data=43414e31
health bits=0x1f ... can=45/45 irq=45 cerr=0 ... supervisor=ok wd=ok
```

通过条件：

- `can=TX/RX` 的发送、接收计数相等并逐秒递增；
- `irq` 与接收帧数同步增长；
- `cerr=0`、`qdrop=0`、`edrop=0`；
- `health bits=0x1f`、`supervisor=ok`、`wd=ok` 持续稳定。

该结果证明 MCU 内部 CAN 控制器、位时序、过滤器、FIFO、中断、FreeRTOS 任务通知和日志链路可用，但不证明 TJA1040、CANH/CANL、ACK、Bus-Off 及外部节点通信已经通过。

## 后续外部总线验收

具备 USB-CAN 或第二个 CAN 节点后，再切换到正常模式并验证实际收发、ACK、错误计数、终端电阻和 Bus-Off 恢复；内部回环版本保留为回归测试基线。
