# DESIGN.md —— 六轴机械臂代码详解 PPT（深色科技风）

## 1. 画布与全局母版（三区）

| 母版区 | 垂直位置 | 高度 | 内容规则 |
| :--- | :--- | :--- | :--- |
| A · 标题块 | 0–120px | 120px | 主标题 32–38px bold，左对齐，含章节序号小标 |
| B · 内容区 | 120–660px | 540px | 正文/代码/图表/卡片 |
| C · 页脚条 | 660–720px | 60px | 左"六轴机械臂代码详解" + 右页码 `NN / 41` |

- 画布 1280×720，padding 上下 20、左右 64px。
- 封面/章节扉页/结束页允许自定义版式，省略 C 区。

## 2. 颜色系统（≤ 4 hex）

| 角色 | hex | 用途 | 面积 |
| :--- | :--- | :--- | :--- |
| 背景 | `#0B1120` | 页面底色、大色块 | ≤ 60% |
| 主色 | `#3B82F6` | 标题、主视觉、卡片头 | ≤ 30% |
| 辅色 | `#06B6D4` | 图表第二系列、分隔线、代码强调 | ≤ 25% |
| 强调色 | `#FBBF24` | 焦点数字、关键注释、CTA | ≤ 10%（Hero 可达 15-20%） |
| 文本主 | `#E2E8F0` | 正文 | 剩余 |
| 文本次 | `#94A3B8` | 注释、页脚 | 剩余 |

渐变：`linear-gradient(135deg, #3B82F6 0%, #06B6D4 100%)`（卡片头/章节号）。半透明：`rgba(59,130,246,0.08)` 大色块。代码块统一 `github-dark` 主题。

## 3. 字体系统

- 中文标题/正文：思源黑体（Source Han Sans / 系统黑体兜底）。
- 西文/代码：`JetBrains Mono`（代码块、锚点数字、模式字符 `#,$,<,S`）。
- 字号阶梯：封面主标 64 / 章节大字 72 / 巨型锚点 72–110 / 页标题 34 / 卡小标 24 / 正文 22 / 代码 15-16 / 脚注 14。
- 代码块 `fontSize: 15`，`language='c'`，`theme='github-dark'`。

## 4. 信息密度与留白

- 常规内容页：字数 ≥ 180，留白 ≤ 35%，主视觉占 B 区 ≥ 30%。
- 代码页：CodeBlock + 右侧/下方拆解文字，填充率 ≥ 85%。
- Hero 页允许留白 35-45%，围绕核心锚点。

## 5. 配图系统

- L1 主视觉：数据流 SVG、双通道 SVG、时序 SVG、掩码 SVG、注入 SVG、状态机 SVG（均为结构化图，用内联 `<svg>`）。
- L3 角标：章节序号小标（统一位置 A 区右上或页脚左）。
- 全部图为 SVG（结构化/低语义），不调用 ImageGen。

## 6. 页面映射表（逐页对照契约）

| # | 文件 | 角色 | 版式 | 主视觉 | 留白% | 色彩分配 |
| - | - | - | - | - | - | - |
| 01 | 01_cover | hero | 全屏+大标题 | SVG 背景 | 35 | 主60+辅30+强调10 |
| 02 | 02_catalog | supporting | 左标题+右内容 | — | 25 | 主40+辅25 |
| 03 | 03_section1 | transition | 章节大字 | 章节号 | 40 | 主50+强调15 |
| 04 | 04_platform | supporting | 非对称双栏 | 参数卡 | 28 | 主45+辅20 |
| 05 | 05_databus | hero | 上大图+下注解 | 数据流 SVG | 30 | 主45+辅20+强调10 |
| 06 | 06_dualchan | supporting | 左大图+右文字 | 双通道 SVG | 25 | 主45+辅25 |
| 07 | 07_instr | supporting | 左标题+右内容 | 模式表 | 22 | 主40+辅25 |
| 08 | 08_struct | supporting | 非对称双栏 | 结构体示意 | 25 | 主45+辅20 |
| 09 | 09_section2 | transition | 章节大字 | 章节号 | 40 | 主50+强调15 |
| 10 | 10_init_overview | supporting | 左标题+右内容 | 十步条 | 22 | 主40+辅25 |
| 11 | 11_swj | supporting | 非对称双栏 | CodeBlock | 25 | 主45+辅20 |
| 12 | 12_clock | supporting | 非对称双栏 | CodeBlock | 25 | 主45+辅20 |
| 13 | 13_gpio | supporting | 非对称双栏 | CodeBlock | 25 | 主45+辅20 |
| 14 | 14_flash | supporting | 非对称双栏 | CodeBlock | 25 | 主45+辅20 |
| 15 | 15_usart | supporting | 非对称双栏 | CodeBlock | 24 | 主45+辅20 |
| 16 | 16_servoinit | peak | 非对称双栏 | CodeBlock | 22 | 主45+辅20+强调8 |
| 17 | 17_tim2init | supporting | 非对称双栏 | CodeBlock | 25 | 主45+辅20 |
| 18 | 18_ps2init | supporting | 非对称双栏 | CodeBlock | 24 | 主45+辅20 |
| 19 | 19_section3 | transition | 章节大字 | 章节号 | 40 | 主50+强调15 |
| 20 | 20_loop | supporting | 非对称双栏 | 任务卡 | 24 | 主45+辅20 |
| 21 | 21_ledkey | supporting | 非对称双栏 | CodeBlock | 25 | 主45+辅20 |
| 22 | 22_dispatch | hero | 非对称双栏 | 路由 SVG | 28 | 主45+辅20+强调10 |
| 23 | 23_isr | supporting | 左大图+右文字 | 状态机 SVG | 25 | 主45+辅25 |
| 24 | 24_parsecmd | supporting | 左标题+右内容 | 命令表 | 20 | 主40+辅25 |
| 25 | 25_parseaction | hero | 上大图+下代码 | CodeBlock | 26 | 主45+辅20+强调10 |
| 26 | 26_parseangle | supporting | 非对称双栏 | CodeBlock | 25 | 主45+辅20 |
| 27 | 27_section4 | transition | 章节大字 | 章节号 | 40 | 主50+强调15 |
| 28 | 28_save | supporting | 非对称双栏 | CodeBlock | 24 | 主45+辅20 |
| 29 | 29_loopaction | supporting | 非对称双栏 | CodeBlock | 24 | 主45+辅20 |
| 30 | 30_dogroup | supporting | 非对称双栏 | CodeBlock | 24 | 主45+辅20 |
| 31 | 31_helper | supporting | 非对称双栏 | CodeBlock | 24 | 主45+辅20 |
| 32 | 32_section5 | transition | 章节大字 | 章节号 | 40 | 主50+强调15 |
| 33 | 33_timirq | hero | 上大图+下注解 | 时序 SVG | 28 | 主45+辅20+强调10 |
| 34 | 34_servoset | supporting | 非对称双栏 | CodeBlock | 24 | 主45+辅20 |
| 35 | 35_inc | supporting | 非对称双栏 | CodeBlock | 25 | 主45+辅20 |
| 36 | 36_ps2read | supporting | 非对称双栏 | 流程 SVG | 25 | 主45+辅20 |
| 37 | 37_edge | hero | 上大图+下注解 | 掩码 SVG | 28 | 主45+辅20+强调10 |
| 38 | 38_inject | hero | 非对称双栏 | 注入 SVG | 28 | 主45+辅20+强调10 |
| 39 | 39_highlight | supporting | 非对称双栏 | 亮点卡 | 22 | 主40+辅25 |
| 40 | 40_faq | supporting | 左标题+右内容 | FAQ 卡 | 22 | 主40+辅25 |
| 41 | 41_ending | hero | 全屏金句 | 收束 | 40 | 主50+强调15 |
