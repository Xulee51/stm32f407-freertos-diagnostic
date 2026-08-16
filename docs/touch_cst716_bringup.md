# 节点 3：CST716 触摸轮询验收

节点 3 使用旧工程中已验证的 CST716 协议，但将驱动独立放入新工程：

- `Core/Src/cst716.c`
- `Core/Inc/cst716.h`

## 接口合同

```text
SCL  PB0
SDA  PF11（软件 I2C，读写阶段切换 GPIO 方向）
INT  PB1（本节点保留为输入，暂不使用 EXTI）
RESET PC13，低电平复位
写地址 0x2A，读地址 0x2B（8 位总线字节值）
```

驱动复用了旧工程确认的寄存器：触摸点数 `0x02`，第一点坐标 `0x03`，版本 `0xA1`，设备 ID `0xA7`。第一版只读取第一个触摸点，不做校准，也不写入外部 Flash/EEPROM。

## 串口验收

初始化成功时应看到：

```text
[TP] CST716 init OK version=0x.... poll=on
```

手指按下屏幕时，USART1 应出现一条：

```text
touch press x=... y=... fingers=1
```

抬起时应出现：

```text
touch release
```

触摸轮询发生在 `ui_task`，周期约 50 ms；触摸失败不会让监督任务停止，只会保留 `[TP] CST716 init FAILED` 或暂时没有坐标事件。这样能先区分 I2C/复位/寄存器故障，再决定是否改用 PB1 EXTI。

## 节点 3 通过条件

- 启动日志出现 CST716 初始化成功和版本号；
- 静止屏幕时不持续刷触摸日志；
- 每次按下/抬起各产生一条事件；
- 坐标在当前 LCD 方向的有效范围内；
- 原有 `health bits=0x07`、heap、栈高水位和 IWDG 监督继续稳定。
