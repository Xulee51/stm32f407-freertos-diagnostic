# 节点 2：ILI9806/FSMC 最小验收

节点 2 只验证 LCD，不接入 CST716。驱动位置：

- `Core/Src/lcd_ili9806.c`
- `Core/Inc/lcd_ili9806.h`

启动顺序是：USART1 启动日志 → FSMC Bank4 初始化 → ILI9806 初始化 → 清屏蓝色 → 显示 `ILI9806 OK` 和三条彩色横条 → 创建 FreeRTOS 任务。

默认配置保持旧工程的已知方向：

```text
LCD_ILI9806_LANDSCAPE=0       # 竖屏，480x800
LCD_BACKLIGHT_ACTIVE_HIGH=0   # 默认按旧工程低电平背光假设
```

这两个默认值不是永久结论。若实机确认相反，在 Git Bash 重新配置时传入：

```bash
cmake --preset debug \
  -DLCD_ILI9806_LANDSCAPE=1 \
  -DLCD_BACKLIGHT_ACTIVE_HIGH=1
cmake --build --preset debug
```

串口应在启动后看到：

```text
[BOOT] freertos_robot_diagnostic
[BOOT] STM32F407ZGT6 168 MHz, USART1 115200
[LCD] ILI9806 FSMC init OK, test pattern written
```

随后健康日志仍应每秒输出。第一次可能是 `health bits=0x01`，待 `ui_task` 运行后应稳定为 `0x07`。若 LCD 初始化失败，串口会输出：

```text
[LCD] ILI9806 FSMC init FAILED
```

此时先保留串口日志，不要立即调整 FreeRTOS 监督逻辑；按“FSMC 时钟/引脚 → Bank4 地址 → 背光极性 → ILI9806 时序”的顺序定位。

本节点只完成编译验证。烧录会覆盖 MCU 当前固件，必须由用户明确确认后再执行 `cmake --build --preset debug --target flash`。
