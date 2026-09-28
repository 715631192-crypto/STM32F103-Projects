# STM32F103C8T6 引脚与外设配置

硬件：STM32F103C8T6（LQFP48），HSE 8 MHz，SYSCLK 72 MHz，APB1 36 MHz，
APB2 72 MHz。引脚定义同时写在 `firmware/stm32/include/smart_lock_pinmap.h`
（板级层用）与 `firmware/stm32/include/smart_lock_board_config.h`（可移植
常量）里，本文件是给人看的核对清单。

## 引脚表

| 模块 | MCU 外设 | 引脚与参数 |
|---|---|---|
| ESP8266（WiFi / OneNET） | USART1 | PA9 TX、PA10 RX，115200 8N1 |
| 指纹模块（AS608 / ZW101） | USART2 | PA2 TX、PA3 RX，57600 8N1 |
| 调试串口（printf） | USART3 | PB10 TX、PB11 RX，115200 8N1 |
| MFRC522 读卡 | SPI1 Mode 0 | PA4 CS、PA5 SCK、PA6 MISO、PA7 MOSI；**PC14 = RST** |
| W25Q64 Flash | SPI2 Mode 0 | PB12 CS、PB13 SCK、PB14 MISO、PB15 MOSI |
| OLED (SSD1306 0x78) + DS3231 (0xD0) | 软件 I2C | PB8 SCL、PB9 SDA |
| SG90 舵机（锁体） | TIM3 CH3 PWM | PB0，50 Hz；0.5 ms 锁闭、1.5 ms 开启 |
| 有源蜂鸣器 | GPIO | PC15，默认低电平关闭 |
| RGB 指示灯 | GPIO | 红 PA0、绿 PA1、蓝 PB1 |
| 4×4 键盘**行** | GPIO 推挽输出 | PA11、PA12、PA15、PB3（扫描时逐行拉低） |
| 4×4 键盘**列** | GPIO 上拉输入 | PB4、PB5、PB6、PB7（按下读到低） |
| 唤醒键 | EXTI8 | PA8，上拉，下降沿唤醒 |
| SWD 调试 | SWD | PA13 SWDIO、PA14 SWCLK |
| 门磁 | —— | **未启用**（PC13 预留，本轮不接） |
| 防撬开关 | —— | **未启用**（PC14 已占用为 RC522 RST） |

## 时钟与引脚复用注意

- PA15、PB3、PB4 原属 JTAG。固件只保留 SWD，`sys_init()` 里调用
  `GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE)` 把这三个脚
  释放成普通 GPIO，键盘才能用它们。
- **4×4 键盘接线必须按键盘板排针的丝印 `R1~R4` / `C1~C4` 接，不能按插针
  物理位置直插**。行 = PA11/PA12/PA15/PB3，列 = PB4/PB5/PB6/PB7，与参考
  工程 `HARDWARE/key_4x4.c` 完全一致。行序就是扫描顺序，接反会导致上下镜像
  （按 1 出 4、按 7 出 *）。键位表见 `firmware/app/src/keypad_input.c`。
- PB2 = BOOT1，必须悬空或下拉，**不要占用**，否则破坏串口救援流程。
- 全片 GPIO 已 100 % 占用（PA0~PA15、PB0~PB15、PC13/PC14/PC15 都有归属），
  因此"防撬开关"没有可用引脚，本轮板级不提供防撬输入。
- 门磁（PC13）与防撬（PC14）在 `smart_lock_board_config.h` 里由
  `SMART_LOCK_HAS_DOOR_SENSOR` / `SMART_LOCK_HAS_TAMPER_SENSOR` 置 0 关闭：
  板级不注册 EXTI，也不产生 `BOARD_DOOR_*` / `BOARD_TAMPER_TRIGGERED` 事件。
  上位应用层对这些事件的处理逻辑保持完整，日后接线只需把开关置 1 并补上
  GPIO/EXTI 初始化，无需改应用层。

## 供电约束

- SG90 必须使用独立 5 V、峰值至少 1 A 的电源，不能由开发板 3.3 V 供电。
- ESP8266 使用独立稳定 3.3 V、峰值至少 500 mA 的电源。
- MFRC522、OLED、W25Q64 和 MCU 均为 3.3 V 逻辑。
- 软件 I2C 已开到约 100 kHz 级别（`SOFT_I2C_DELAY_LOOPS`），线较长时须
  提高延时或降到 50 kHz。
- 所有电源共地；舵机电源入口加大容量电容，并与 MCU 去耦分开布置。

## 低功耗策略

`SMART_LOCK_ENABLE_STOP_MODE` 默认为 **0**，`board_try_stop_mode()` 直接
返回 false（即"没有真的睡下去"），上层照常刷新活跃时间。原因是进入 STOP
后唤醒时钟回到 HSI，必须重新 `SystemInit()` 并恢复 UART/I2C/SPI，否则
ESP8266 与指纹模块会失步。

启用 STOP 的前置条件（见 `docs/bring-up.md`）：

1. 先把 `SMART_LOCK_ENABLE_STOP_MODE` 与 `configUSE_TICKLESS_IDLE` 同时置 1；
2. 进 STOP 前关闭 ESP8266、指纹模块、OLED 和 RC522 电源，保持锁体机械锁闭；
3. 用 PA8 唤醒并验证唤醒后 72 MHz PLL 与外设时钟恢复、各总线不失步。
