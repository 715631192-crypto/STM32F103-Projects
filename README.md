# STM32F103 嵌入式项目合集

本仓库收录 **四个基于 STM32F103（Cortex-M3）** 的嵌入式软硬件项目，覆盖传感器采集、执行器控制、飞控姿态解算与物联网安全等方向。四个项目均为独立可编译的完整工程，由本人从零搭建 / 主导开发。

> 提交策略：按“精简提交”处理——剔除构建产物（`*.o/*.d/*.crf/*.axf` 等）、与 F103 无关的 MDK 通用 ARM 设备树（`ARMCA*/ARMCM*/ARMv8*` 等）、`unpackage/` 打包产物、`.rar` 备份、二进制资源（`.png/.apk/.keystore` 等）以及 WorkBuddy 内部目录（`.slidep/.workbuddy`）。工程自有代码、STM32 标准外设库（StdPeriph）、FreeRTOS、CMSIS 核心头文件与全部文档均保留，仓库基本可直接编译。

## 项目一览

| 目录 | 项目 | 核心芯片 / 外设 | 技术要点 |
| --- | --- | --- | --- |
| `037-DHT11` | 环境温湿度监测系统 | DHT11 / ESP8266 / STM32F103C8 | 单总线时序、OneNET MQTT 上云、Qt 串口助手、uni-app 小程序 |
| `038-机械臂` | 6 轴舵机机械臂 | 总线舵机 / PS2 手柄 / STM32F103 | TIM2 分时复用 PWM、GPIO 模拟 SPI、六自由度逆运动学、OpenMV 视觉、QT 上位机 |
| `040-四轴飞行器` | 四轴飞行器飞控 | MPU6050 / PMW3901 / NRF24L01 | 四元数姿态解算、串级 PID、18 kHz PWM、光流定点 |
| `041-智能门锁` | 金融级安全物联网门锁 | AS608 / RC522 / ESP8266 / FreeRTOS | TOTP/HMAC-SHA1 动态口令、防重放、5 任务调度 |

## 通用说明

- **工具链**：Keil MDK（`.uvprojx`）或 EIDE / CMake + `arm-none-eabi-gcc`。各工程已附工程文件。
- **芯片**：均为 STM32F103 系列（C8T6 / CBT6 等），主频 72 MHz。
- **第三方库**：StdPeriph 标准外设库、CMSIS 核心头文件、FreeRTOS（仅 041）随仓库提供；若本地缺失，请参照各项目 README 从 STM32 标准外设库 / CMSIS 包获取对应版本。
- **字符编码**：源码与文档使用 UTF-8（含中文注释）。

## 目录结构

```
STM32F103-Projects/
├── 037-DHT11/       温湿度监测（DHT11 + ESP8266 + OneNET）
├── 038-机械臂/       6 轴舵机机械臂（PS2 + 总线舵机）
├── 040-四轴飞行器/   四轴飞控 + 遥控端
├── 041-智能门锁/     安全物联网门锁（FreeRTOS + 云端）
├── README.md        本文件
├── .gitignore
└── LICENSE
```

## 许可证

本项目以 [MIT 许可证](./LICENSE) 开源。
