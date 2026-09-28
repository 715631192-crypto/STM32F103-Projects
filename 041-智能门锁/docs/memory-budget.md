# STM32F103C8T6 内存预算

## Keil / ARM Compiler 5 实测（可烧录固件）

`SmartLock.uvprojx` 全量 Rebuild（ARM Compiler 5 `V5.06 update 7 build 960`，
`-O1`）的链接结果：

```
Program Size: Code=55776  RO-data=5896  RW-data=400  ZI-data=17672
0 Error(s), 0 Warning(s)
```

| 资源 | 已用 | 容量 | 余量 |
|---|---:|---:|---:|
| Flash（Code + RO-data + RW-data） | 62072 B | 65536 B | 3464 B（5.3%） |
| SRAM（RW-data + ZI-data） | 18072 B | 20480 B | 2408 B（11.8%） |

> **本轮净减 1.5 KB Flash，同时还加进了一整条功能**，靠的是把中文字库从
> 151 字裁到实际用到的 73 字：
>
> | 项 | 变化 | 说明 |
> |---|---:|---|
> | 中文字库点阵 + 码点表 | **−2652 B** | 151→73 字（`gen_oled_font.py --trim`） |
> | 应用层目标码 | +1100 B | 改主密码向导 + 分步提示语 + 新审计文案 |
> | RW/ZI | +16 / +8 B | 向导的 4 个静态变量（含 `char[12]` 新密码暂存） |
> | **合计** | **−1540 B** | 63612 → 62072 B |
>
> 裁剪**只做过滤**：73 字里有 72 字的点阵字节与原字库逐字节相同，只有界面上
> 原先没有的「原」字是新渲染的。详见 `tools/gen_oled_font.py` 的 `--trim`。
> 下一次再缺 Flash，可继续走这条路（当前 73 字已是 UI 实际用字，实测无冗余）。

参考工程（同一颗芯片、同一编译器）的对应值为
`Code=53076 RO-data=7848 RW-data=484 ZI-data=19844`，即 SRAM 已用掉
20328/20480 B —— 只剩 152 B。本工程代码量与它基本持平，**任务栈与队列改为
全部静态分配**，所以不能再保留参考工程的 16 KB 动态堆。

## SRAM 明细

| 组成 | 字节 |
|---|---:|
| FreeRTOS 动态堆 `ucHeap` | 4096 |
| 5 个应用任务静态栈（256/384/320/256/160 字） | 5504 |
| 空闲任务 + 软件定时器任务栈与控制块 | 1688 |
| 启动文件 MSP 栈（`Stack_Size = 0x400`） | 1024 |
| USART1/2/3 接收环形缓冲（512/256/128） | 908 |
| esp8266 线路缓冲与下行缓冲 | 680 |
| `board_port.c` 静态状态 | 576 |
| FreeRTOS 内核控制块（TCB / 队列 / 堆元数据） | 约 600 |
| 其余（DS3231 / OLED / 定时器 / RCC 等） | 约 350 |

## Flash 明细

中文字库仍是最大单项，但已瘦身：`oled.o` 的 RO-data 由 7224 B 降到约 4568 B，
其中 `oledfont_cn.h` 占 73×32 B 点阵 + 73×2 B 码点 = 2482 B，`oledfont.h` 的
ASCII 8x16 / 6x8 占 2090 B。其余为应用层目标码（`cjson.o`、`tasks.o`、
`queue.o`、`board_port.o`、`smart_lock_app.o` 等；其中 `board_port.o` 因
包含键盘扫描、RC522 自愈与全部界面字符串而偏大）。

## 动态堆只服务三个瞬时调用方

`configTOTAL_HEAP_SIZE` 现为 **4 KB**，使用者只有：

1. `app_wifi.c` —— 通过 `cJSON_InitHooks` 把 cJSON 的 `malloc/free` 接到
   `pvPortMalloc` / `vPortFree`（否则 microlib 堆为 0，`cJSON_Parse` 必失败）。
2. MqttKit —— `MQTT_NewBuffer` / `MQTT_UnPacketPublish` 等报文缓冲。
3. `esp8266.c` —— `mqtt_on_packet` 下行解包。

峰值估算：下行 ≤ `MQTT_RX_MAX`(512) + cJSON 解析一棵几十字节级 JSON
（约 0.5–1 KB 节点）+ 一次上行 publish 缓冲（topic 96 + payload ≤192）。
约 1.5–2 KB，故 4 KB 留了约 2 倍余量。

## 上板后仍需实测（不要只依赖估算）

- `uxTaskGetStackHighWaterMark()` 逐个任务校验栈深度。
  参考工程的静态调用链分析显示最深路径约 1224 B（`on_mqtt_msg` 链），
  本工程 network 任务栈为 320 字 = 1280 B，余量偏薄，**优先验证这一条**。
- `xPortGetFreeHeapSize()` / `xPortGetMinimumEverFreeHeapSize()` 校验动态堆峰值。
- 若动态堆吃紧，第一顺位是关掉未使用的 FreeRTOS 软件定时器任务
  （`configUSE_TIMERS = 0`；本工程当前没有任何 `xTimer*` 调用，
  可回收约 1.5–1.8 KB 栈与队列），**不要**压缩安全任务栈。
- 若 Flash 吃紧，首选执行 `python tools/gen_oled_font.py --check` 确认字库
  没被改脏，再用 `--trim` 裁掉界面上用不到的字（当前已是 73/73 字，
  即已经裁无可裁），**不要**砍安全逻辑。

## 校验产物的方法（不要用 hex 的 MD5）

`HARDWARE/ds3231.c` 用 `__DATE__` / `__TIME__` 生成编译期 RTC 基准时间，
于是**每次构建的 hex 都不同**（实测：源码一字未改、连做两次 `-r`，
`Program Size` 完全相同但 hex 的 MD5 不同）。所以：

| 想确认 | 可靠信号 | 不可靠信号 |
|---|---|---|
| 真的重新编译过 | 构建日志里出现全部 `compiling *.c...` 行 | hex MD5 变了 |
| 真的重新链接过 | 日志里出现 `Program Size:` 行 | hex 文件大小变了 |
| 结果正确 | `0 Error(s), 0 Warning(s)` + 工具链退出码 0 | — |

> 附带坑：**UV4 偶发启动失败**（`Start-Process -PassThru` 拿到空退出码、
> 日志文件根本没生成、`obj/` 时间戳不动）。看到"没日志 + 空退出码"就是
> 没跑起来，直接重试（建议 `try/catch` + 最多重试 3 次），不要当成构建成功。

## GCC 侧的口径差异

`arm-none-eabi-gcc` 的编译校验目标（`-DSMART_LOCK_BUILD_FIRMWARE_SYNTAX=ON`）
只做类型与签名检查，不链接固件，因此上面的数字均以 Keil/AC5 为准。
