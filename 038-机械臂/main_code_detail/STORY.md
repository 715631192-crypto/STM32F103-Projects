# STORY.md —— 六轴机械臂 STM32F103 主控代码详解（从 main 函数讲起）

## ① 用户意图对齐

- **目标受众**：嵌入式开发者 / 课程学员 / 项目接手者；用于代码讲解、内训、技术复盘。
- **核心目标**：观众能顺着 `main()` 的执行顺序，读懂每个初始化步骤与每个被调用的函数的职责、参数、内部逻辑与相互调用关系；理解"一条数据主线 + 双舵机通道"的整体架构。
- **PPT 长度**：约 41 页（5 章 + 封面/目录/结束），属于"详细讲解"档，每页都带代码片段与逐行拆解。
- **视觉调性**：深色科技风、克制、重逻辑、代码优先。
- **内容边界（必讲）**：`main()` 初始化十步、主循环五任务、`app_usart_run` 指令分发、串口中断状态机、`parse_cmd`/`parse_action`/`parse_angle`/`save_action`、`loop_action`/`do_group_once`/`getMaxTime`、TIM2 分时 PWM、`parse_psx_buf` 边沿检测与"伪装串口"、PS2 底层 GPIO 模拟 SPI。
- **禁碰**：硬件原理图绘制、Keil 工程配置、上位机软件用法、与代码无关的科普。

## ② 页面布局骨架

- **目录声明 5 章**，对应 5 个 `type: section` 章节扉页（编号 03/09/19/27/32，连续，与目录逐字一致）：
  1. 系统全景：一条数据主线（扉页 03）
  2. main() 初始化十步详解（扉页 09）
  3. 主循环与指令分发（扉页 19）
  4. 动作组：保存·回放·执行（扉页 27）
  5. 波形输出与 PS2 手柄控制（扉页 32）
- **Hero 页（视觉高潮）**：01 封面、05 数据主线流程图、22 指令分发中枢、25 parse_action 核心、33 分时 PWM 时序图、37 边沿检测、38 伪装串口、41 结束页。（≈ 8/41 ≈ 20%，落在 20-30% 区间）
- **rhythm 曲线**：cover(peak) → 02(valley) → 03(transition) → 04-08(valley) → 05(peak) → 06-08(valley) → 09(transition) → 10-18(valley，中间 16 小 peak) → 19(transition) → 20-21(valley) → 22(peak) → 23-24(valley) → 25(peak) → 26(valley) → 27(transition) → 28-31(valley) → 32(transition) → 33(peak) → 34-36(valley) → 37(peak) → 38(peak) → 39-40(valley) → 41(peak)。
- **非对称版式**：≥ 40% 页面使用（左大图+右文字 / 非对称双栏 / 巨型数字 / 上大图+下卡片），对称版式仅 02 目录、07 指令总表等少数页。
- **相邻页版式不重复**，连续 ≥2 页对称必须打断。

## ③ 页面大纲（41 页）

| # | title | type | role | rhythm | layout | visual | visual_role | density | anti_pattern |
| - | - | - | - | - | - | - | - | - | - |
| 01 | 六轴机械臂主控代码详解 | cover | hero | peak | 全屏视觉+大标题 | SVG 电路风背景 | atmosphere | 标题+副标 | 禁止等宽卡片 |
| 02 | 讲解目录 | catalog | supporting | valley | 左标题+右内容(五章) | L3 角标 | — | 字数≥120 | 禁止四卡预览 |
| 03 | 第1章 系统全景 | section | transition | transition | 全屏章节大字 | 章节号 | anchor | 30 | 禁止铺满正文 |
| 04 | 项目与硬件平台 | content | supporting | valley | 非对称双栏(左概述右参数) | 参数卡 | evidence | 200 | 禁止等分双栏 |
| 05 | 一条数据主线 | content | hero | peak | 上大图(数据流SVG)+下注解 | SVG 数据流 | anchor | 160 | 禁止卡片横排 |
| 06 | 双舵机通道 | content | supporting | valley | 左大图+右文字 | SVG 双通道 | anchor | 220 | 禁止 50:50 |
| 07 | 指令系统总览（5 种模式） | content | supporting | valley | 左标题+右内容(表) | 模式对照表 | evidence | 300 | 禁止图表卡 |
| 08 | 舵机运动结构体 | content | supporting | valley | 非对称双栏(结构+原理) | 结构体示意 | evidence | 240 | 禁止等分 |
| 09 | 第2章 main 初始化十步 | section | transition | transition | 全屏章节大字 | 章节号 | anchor | 30 | 禁止铺满正文 |
| 10 | 初始化总览（十步顺序） | content | supporting | valley | 左标题+右内容(步骤) | 十步条 | evidence | 320 | 禁止四卡 |
| 11 | ① SWJ_GPIO_Init 释放调试脚 | content | supporting | valley | 非对称双栏(代码+说明) | CodeBlock | evidence | 220 | 禁止等分 |
| 12 | ② rcc_Init 与 ③ SysTick_Init | content | supporting | valley | 非对称双栏 | CodeBlock | evidence | 220 | 禁止等分 |
| 13 | ④ app_gpio_init 与 ⑤ app_setup_start | content | supporting | valley | 非对称双栏 | CodeBlock | evidence | 220 | 禁止等分 |
| 14 | ⑥ spi_flash_init W25Q64 初始化 | content | supporting | valley | 非对称双栏 | CodeBlock | evidence | 220 | 禁止等分 |
| 15 | ⑦ app_usart_init 双串口 | content | supporting | valley | 非对称双栏(代码+脚) | CodeBlock | evidence | 240 | 禁止等分 |
| 16 | ⑧ servo_init PWM+总线回中 | content | supporting | peak | 非对称双栏(代码+修复) | CodeBlock | evidence | 260 | 禁止等分 |
| 17 | ⑨ TIM2_init 时基 | content | supporting | valley | 非对称双栏(代码+参数) | CodeBlock | evidence | 220 | 禁止等分 |
| 18 | ⑩ app_ps2_init 手柄初始化 | content | supporting | valley | 非对称双栏(序列+说明) | CodeBlock | evidence | 240 | 禁止等分 |
| 19 | 第3章 主循环与指令分发 | section | transition | transition | 全屏章节大字 | 章节号 | anchor | 30 | 禁止铺满正文 |
| 20 | 主循环总览 while(1) | content | supporting | valley | 非对称双栏(循环+任务) | 任务卡 | evidence | 240 | 禁止等分 |
| 21 | app_led_run / app_key_run | content | supporting | valley | 非对称双栏 | CodeBlock | evidence | 220 | 禁止等分 |
| 22 | app_usart_run 指令分发中枢 | content | hero | peak | 非对称双栏(mode→函数) | 路由 SVG | anchor | 240 | 禁止卡片横排 |
| 23 | 串口中断状态机 uart1_mode | content | supporting | valley | 左大图+右文字(状态机) | SVG 状态机 | anchor | 250 | 禁止 50:50 |
| 24 | parse_cmd 系统命令全集 | content | supporting | valley | 左标题+右内容(命令表) | 命令表 | evidence | 340 | 禁止图表卡 |
| 25 | parse_action 舵机移动解析 | content | hero | peak | 上大图(SVG)+下代码 | CodeBlock | anchor | 260 | 禁止卡片横排 |
| 26 | parse_angle 角度模式 | content | supporting | valley | 非对称双栏(代码+换算) | CodeBlock | evidence | 220 | 禁止等分 |
| 27 | 第4章 动作组 保存·回放·执行 | section | transition | transition | 全屏章节大字 | 章节号 | anchor | 30 | 禁止铺满正文 |
| 28 | save_action 动作组保存 | content | supporting | valley | 非对称双栏(代码+帧式) | CodeBlock | evidence | 260 | 禁止等分 |
| 29 | loop_action 循环执行器 | content | supporting | valley | 非对称双栏(代码+修复) | CodeBlock | evidence | 240 | 禁止等分 |
| 30 | do_group_once 与 getMaxTime | content | supporting | valley | 非对称双栏 | CodeBlock | evidence | 240 | 禁止等分 |
| 31 | print_group / rewrite_eeprom / replace_char | content | supporting | valley | 非对称双栏(三函数) | CodeBlock | evidence | 240 | 禁止等分 |
| 32 | 第5章 波形输出与 PS2 控制 | section | transition | transition | 全屏章节大字 | 章节号 | anchor | 30 | 禁止铺满正文 |
| 33 | TIM2_IRQHandler 分时 PWM | content | hero | peak | 上大图(时序SVG)+下注解 | SVG 时序 | anchor | 240 | 禁止卡片横排 |
| 34 | servo_pin_set / duoji_doing_set | content | supporting | valley | 非对称双栏(代码+钳位) | CodeBlock | evidence | 240 | 禁止等分 |
| 35 | servo_inc_offset 平滑步进 | content | supporting | valley | 非对称双栏 | CodeBlock | evidence | 220 | 禁止等分 |
| 36 | PS2 读取流程 app_ps2_run | content | supporting | valley | 非对称双栏(流程+代码) | SVG 流程 | evidence | 230 | 禁止等分 |
| 37 | parse_psx_buf 边沿检测 | content | hero | peak | 上大图(掩码SVG)+下注解 | SVG 掩码 | anchor | 230 | 禁止卡片横排 |
| 38 | parse_psx_buf 伪装串口注入 | content | hero | peak | 非对称双栏(流程+代码) | SVG 注入 | anchor | 240 | 禁止卡片横排 |
| 39 | 架构总结与设计亮点 | content | supporting | valley | 非对称双栏(亮点) | 亮点卡 | evidence | 300 | 禁止等分 |
| 40 | 常见问题与调试要点 | content | supporting | valley | 左标题+右内容(FAQ) | FAQ 卡 | evidence | 320 | 禁止四卡 |
| 41 | 结束页 | ending | hero | peak | 全屏金句 | 收束 | anchor | 20 | 禁止铺满正文 |
