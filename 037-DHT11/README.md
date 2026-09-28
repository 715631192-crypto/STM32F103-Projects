# 037-DHT11：基于 STM32F103 的环境温湿度监测系统

## 项目背景

一套完整的“端—云—端”温湿度监测方案：下位机以 STM32F103 采集 DHT11 温湿度，经 ESP8266 通过 OneNET MQTT 协议上云；配套 **Qt（C++）串口助手**与 **uni-app 小程序**做本地 / 远程查看与控制。适合作为物联网入门与毕业设计级演示。

## 硬件组成

- 主控：STM32F103C8T6（72 MHz）
- 传感器：DHT11 数字温湿度传感器（单总线）
- 通信：ESP8266-01S WiFi 模块（AT 指令，UART 接入 OneNET）
- 人机：0.96" OLED（I2C/SPI）、矩阵按键、LED、蜂鸣器

## 软件架构

| 模块 | 文件 | 说明 |
| --- | --- | --- |
| DHT11 驱动 | `Hardware/DHT11.c/.h` | 单总线起始/响应/40bit 数据解析时序 |
| 串口 | `Hardware/USART.c/.h` | 与 ESP8261 交互的 AT 指令收发 |
| OLED | `Hardware/OLED.c/.h` + `OLED_Font.h` | 本地显示温湿度 |
| 外设 | `Hardware/KEY/LED/BUZZER` | 按键、状态指示、报警 |
| 云平台 | `NET/`（OneNET） | MQTT 连接、JSON 数据上行/命令下行 |
| 上位机 | `DHT11NET/`（uni-app） | 移动端小程序，REST/MQTT 订阅云端数据 |
| 启动 | `Start/`、`Styem/`（System）、`User/` | 启动文件、系统时钟、主循环 |

## 关键技术点

- **DHT11 单总线**：严格的 us 级时序（起始信号 → 响应 → 40 bit 数据），需关闭中断或用定时器精确延时。
- **ESP8266 + OneNET MQTT**：通过 `AT` 指令建立 TCP/MQTT 会话，以 JSON 载荷上报 `temperature` / `humidity`，并接收平台下发的控制指令。
- **PTC 加热 PID**：对传感器做恒温补偿（PTC 加热 + PID 控温），降低低温结露带来的测量误差。
- **Qt 串口助手**：C++/Qt 信号槽架构，实时绘制曲线、下发配置。
- **uni-app 小程序**：跨端（H5/小程序）展示历史数据与告警。

## 目录结构

```
037-DHT11/
├── Hardware/   DHT11/USART/OLED/KEY/LED/BUZZER 驱动
├── Library/    STM32 标准外设库（StdPeriph）
├── MDK-ARM/    工程文件、CMSIS
├── NET/        OneNET MQTT 接入
├── DHT11NET/   uni-app 移动端源码（unpackage/ 打包产物已剔除）
├── Start/      启动文件、core_cm3、stm32f10x.h、system_stm32f10x
├── Styem/      系统层（System）
└── User/       应用主逻辑
```

## 编译与运行

1. 工具：Keil MDK 或 EIDE，芯片选 STM32F103C8，外部晶振 8 MHz（PLL → 72 MHz）。
2. 库：需 STM32 标准外设库与 CMSIS（已随本仓库）；若缺失请从 STM32 标准外设库 V3.5 获取。
3. 烧录后，串口配置 ESP8266 连接 OneNET（填入设备 ID / APIKey）；小程序订阅对应数据流即可查看。
4. `DHT11NET/` 为 uni-app 工程，用 HBuilderX 打开，`unpackage/` 为打包产物（含签名 `*.keystore`）**不上传**。

## 备注

- 仓库已剔除 `unpackage/` 与签名文件，仅保留 uni-app 可编译源码。
