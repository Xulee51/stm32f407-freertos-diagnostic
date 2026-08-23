# 节点 6：FreeRTOS UI 事件层与 LCD 状态页

节点 6 将触摸采集、事件传递和 LCD 刷新拆成清晰的任务边界：

```text
CST716 PB1 EXTI
       |
       v
input_task -- ui_event_queue --> ui_task -- FSMC --> ILI9806
                                      |
                                      +--> log_queue --> logger_task --> USART1
```

## 任务职责

- `input_task`：等待 PB1 通知或 50 ms 超时，读取 CST716，检测按下/释放沿；不访问 LCD。
- `ui_event_queue`：容量 8，传递 `press/release + 坐标 + fingers + tick`；满队列时增加 `edrop`。
- `ui_task`：独占 LCD，每 500 ms 更新一次状态页，并消费触摸事件；不在中断中绘图。
- `logger_task`：继续独占 USART1，输出 UI 事件和健康快照。

## LCD 状态页

状态页显示以下信息：

- `HEALTH`：当前健康事件位；
- `HEAP`：剩余 FreeRTOS Heap；
- `TOUCH`、`X/Y`：最近一次触摸坐标；
- `TYPE`：0=无事件，1=按下，2=释放；
- `EVENT`：UI 任务已消费的事件数；
- `DROP`：UI 事件队列丢弃数。

## 串口验收

节点 6 的健康日志格式为：

```text
health bits=0x0f heap=... min_heap=... qdrop=0 edrop=0 tirq=...
stack_hwm=health/logger/input/ui supervisor=ok wd=ok
```

触摸事件仍应输出：

```text
touch press x=... y=... fingers=1
touch release x=... y=...
```

## 通过条件

- LCD 启动后显示状态页，不覆盖屏幕上方的启动测试区域；
- 连续点击、长按和释放均能更新 LCD 与串口事件；
- `health bits=0x0f` 稳定；
- `qdrop=0`、`edrop=0`；
- `tirq` 只在触摸期间增加，静止时不高速增长；
- `heap`、四个任务栈高水位、`supervisor` 和 `wd` 稳定。

本节点只使用 ASCII 字模和小块局部刷新，不引入字库、图片或 DMA，便于先学习任务间通信和 LCD 访问边界。
