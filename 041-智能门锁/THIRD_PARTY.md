# Third-party component policy

## Vendored components

### FreeRTOS kernel V10.5.1

| 项 | 内容 |
|---|---|
| 位置 | `firmware/FreeRTOS/` |
| 来源 | 参考工程 `STM32_SmartLock/FreeRTOS/` 原样搬入（内核 V10.5.1） |
| 许可证 | MIT，见 `firmware/FreeRTOS/LICENSE.md` |
| 本地改动 | 内核与 Keil 移植层**逐字节保留**，无改动 |

移植层有两份，按编译器分开存放：

- `portable/RVDS/ARM_CM3/` —— 参考工程原版，Keil MDK (AC5/RVDS) 使用。
- `portable/GCC/ARM_CM3/` —— 官方 FreeRTOS-Kernel V10.5.1 的 GCC 移植层。
  本仓库的目标工具链是 `arm-none-eabi-gcc`（见 `CMakePresets.json` 与
  `README.md`），而 RVDS 版含 `__forceinline` 与 RVDS 语法的 `__asm { }`
  块，GCC 无法解析，因此编译器相关的移植层必须换成 GCC 版。两份移植层
  的内核版本均为 V10.5.1，与内核严格配套。
- `portable/MemMang/heap_4.c` —— 内核自带内存管理（最佳适配 + 相邻空闲块合并）。

FreeRTOS 配置在 `firmware/stm32/include/FreeRTOSConfig.h`，以参考工程的
`USER/FreeRTOSConfig.h` 为基准，仅改三处（开启静态分配、关闭 tickless idle、
保留 heap_4 的动态堆），改动理由写在该文件顶部。

## Future third-party integration

计划中还需要引入、但**尚未**进入本仓库的组件：

- 参考工程的 `MIDDLEWARE/`（cJSON、MqttKit、base64、onenet_token、sha1、totp）
  与 `HARDWARE/` 驱动 —— 目前由 CMake 通过 `SMART_LOCK_REF_ROOT` 直接引用，
  未复制进仓库。
- Eclipse Paho Embedded C MQTT 客户端（若最终不走 MqttKit 方案）。

引入时逐项记录**版本、来源 URL、许可证原文、本地改动**，并把厂商生成的
文件与 MIT 许可的应用代码分开存放。

## References only

其它调研过的智能门锁仓库仅作架构参考，未复制代码：其中部分未声明许可证，
另一些面向不同 MCU 或框架。**公开仓库若没有许可证，并不授予再分发权利。**
