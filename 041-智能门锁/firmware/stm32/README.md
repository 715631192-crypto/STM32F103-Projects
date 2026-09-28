# STM32F103C8T6 集成层

`firmware/app` 是可移植、可主机测试的安全核心；`firmware/stm32` 是把它接到
真实硬件上的板级适配层与应用任务层。

## 技术选型（重要）

本工程**不用 STM32CubeMX/HAL**，改用：

| 方面 | 选型 | 说明 |
|---|---|---|
| 外设库 | STM32F10x 标准外设库（StdPeriph） | `USE_STDPERIPH_DRIVER` + `STM32F10X_MD` |
| 驱动 | 参考工程 `STM32_SmartLock/HARDWARE` + `SYSTEM` | 已上板验证：i2c_soft / oled / ds3231 / mfrc522 / w25q64 / servo / buzzer / rgb_led / as608 / esp8266 |
| 云端 | 参考工程 `MIDDLEWARE` + `USER/app_wifi.c` | **原样零改动**，保证 OneNET 物模型与主题一致 |
| RTOS | `firmware/FreeRTOS/`（内核 V10.5.1） | 见下节 |
| 工具链（编译校验） | CMake + `arm-none-eabi-gcc` | 见 `CMakePresets.json` |
| 工具链（烧录固件） | **Keil MDK / ARM Compiler 5** | 根目录 `SmartLock.uvprojx`，见下节 |

`firmware/stm32/cubemx/SmartLock.ioc` 是早期 HAL 方案的遗留文件，**已不作为
构建依据**，仅保留作为引脚历史记录。

## FreeRTOS 从哪来

内核取自参考工程（`firmware/FreeRTOS/`，V10.5.1 原样搬入），移植层分两份：

- `portable/RVDS/ARM_CM3/` —— 参考工程原版，Keil AC5 用户用这份。
- `portable/GCC/ARM_CM3/` —— 官方同版本 GCC 移植层，本仓库的 GCC 构建用这份。

原因：RVDS 版含 `__forceinline` 与 RVDS 语法的 `__asm { }` 块，GCC 无法解析；
而 RVDS 版和本仓库工具链不匹配。两份移植层内核版本一致。详见 `THIRD_PARTY.md`。

配置在 `firmware/stm32/include/FreeRTOSConfig.h`：开启静态分配（应用层全程用
`xTaskCreateStatic` / `xQueueCreateStatic`）、关闭 tickless idle、保留 heap_4
（`configTOTAL_HEAP_SIZE = 4 KB`）供 cJSON/MqttKit/esp8266 解包使用。
堆为什么不是参考工程的 16 KB，见下面「内存预算」。

## Keil MDK 工程（可直接编译出固件）

仓库根目录的 `SmartLock.uvprojx` 是一个**完整可链接**的 Keil MDK 工程，已实测
全量 Rebuild 通过：`0 Error(s), 0 Warning(s)`，并生成 `obj/SmartLock.hex`。

用 Keil 打开 `SmartLock.uvprojx` 点 Build 即可；无头（批处理）验证：

```powershell
# UV4 是 GUI 子系统程序，用 & 调用不会阻塞，必须 Start-Process -Wait
$p = Start-Process -FilePath "D:\Keil_v5\UV4\UV4.exe" `
     -ArgumentList '-r','SmartLock.uvprojx','-j0','-o','build_keil.log' -Wait -PassThru
$p.ExitCode    # 0 = 无错误无告警；1 = 有告警；2 = 有错误
```

工程要点：

| 项 | 取值 |
|---|---|
| Target | `STM32F103C8` |
| 编译器 | ARM Compiler 5（`V5.06 update 7 (build 960)`，`uAC6=0`） |
| 宏 | `USE_STDPERIPH_DRIVER,STM32F10X_MD` |
| FreeRTOS 移植层 | `firmware/FreeRTOS/portable/RVDS/ARM_CM3`（AC5 版） |
| 散度加载 | `STM32F103C8.sct`（IROM 0x08000000/0x10000、IRAM 0x20000000/0x5000） |
| 启动文件 | `startup_stm32f103xb.s` |
| StdPeriph | 直接引用 Keil Pack `Keil\STM32F1xx_DFP\2.3.0` 里的 `.c` 与 `inc` |
| 包含路径顺序 | `firmware/stm32/include` 在首位，本工程同名头优先，避免被参考工程抢 |
| 分组 / 文件数 | 9 组 / 123 个文件（其中参与编译的 C 与汇编共 56 个） |

工程文件由脚本生成，**不要手工维护逐文件列表**：

```powershell
python tools/gen_keil_project.py
```

脚本扫描 `firmware/**` 并带文件存在性预检，缺文件会拒绝生成。改动源码目录
结构后重新跑一次即可。

## 注意：源码必须保存为「UTF-8 带 BOM」

这是本项目在 Keil 下最容易踩的坑。

ARM Compiler 5 **只认 UTF-8 BOM**。含中文但没有 BOM 的源文件，armcc 会按
Windows 系统代码页（简体中文下为 CP936/GBK）解析：3 字节的 UTF-8 汉字被
错位成双字节字符，字符串字面量末尾的右引号被当作汉字的后半字节"吃掉"：

```
firmware\stm32\src\board_port.c(756): error:  #8: missing closing quote
      case LOCK_STATE_LOCKED:   return "宸蹭笂閿?";
```

对照实验（同一份源文件、同一编译器、同一编译选项）：

| 文件 | BOM | 结果 |
|---|---|---|
| 参考工程 `USER/app_ui.c` | 有 | 0 error / 2 warning |
| 同上，仅去掉 BOM | 无 | **30 error** |
| 本工程 `board_port.c` | 无 | **28 error** |
| 同上，仅加上 BOM | 有 | 0 error / 0 warning |

参考工程之所以能编过，是因为它的文件在 uVision 里编辑保存过，而 uVision 写
UTF-8 时自带 BOM。本仓库源码由外部工具生成，所以必须显式补 BOM。

- 检查：`python tools/fix_source_bom.py --check`（有缺失时退出码 1）
- 修复：`python tools/fix_source_bom.py`
- GCC 会忽略 BOM，所以 CMake 的两条构建线不受影响，可以放心统一。
- 在 Keil 里新建或另存文件时，编码选 **Encode in UTF-8**（即带 BOM）。

## OLED 中文字库（`HARDWARE/oledfont_cn.h`）

`OLED_ShowText()` 走 16 px 行高的 UTF-8 渲染，汉字字形来自
`oledfont_cn.h` 的 `CN_CODE`/`CN_FONT` 表。**查不到码点就画一个空心方框**，
所以界面文案里的字必须都在表里，否则屏幕上就是一片方框 —— 上板看到的
"OLED 乱屏"有一半是这个原因。

字库由 `tools/gen_oled_font.py` 维护，它会自己扫描 UI 源码里的字符串字面量
（当前是 `firmware/stm32/src/board_port.c`），算出到底用到哪些汉字：

```bash
python tools/gen_oled_font.py --check        # 只体检，缺字时退出码 1
python tools/gen_oled_font.py                # 缺字就用 Windows 自带 simhei.ttf 补齐
python tools/gen_oled_font.py --trim         # 裁到"实际用到的字"，并顺带补缺字
```

- 生成参数 `size=16, dx=0, dy=-1, thr=120` 是用仓库里已有字形反推出来的，
  与手工取模软件的效果肉眼无差，但**做不到逐位复刻**，所以只用它补缺字，
  不要拿它整体重刷已有字形。
- 新增界面文案后先跑 `--check`：CI/提交前顺手跑一次，就不会再出现"上板才
  发现缺字"。
- `--trim` **只做过滤**：留下的字原封不动沿用表里已有的点阵字节，
  绝不会用 TTF 重渲染，所以同样不会有"肉眼难辨的差异"。当前表里
  73 字 = 界面用字，实测无冗余。
- ⚠ 被 `--trim` 裁掉的字如果日后又被 UI 用到，补齐模式会用 TTF 重渲染它，
  那时它的位图就与最初出模的字库不一致了（仍是"肉眼难辨"级别）。正确做法
  是用版本库里的旧 `oledfont_cn.h` 把那 32 字节换回来，别让它停在 TTF 版本上。

## 内存预算（Keil AC5 实测）

`SmartLock.uvprojx` 全量 Rebuild 的链接结果：

```
Program Size: Code=55776  RO-data=5896  RW-data=400  ZI-data=17672
```

| 资源 | 已用 | 容量 | 余量 |
|---|---:|---:|---:|
| Flash（Code + RO + RW） | 62072 B | 65536 B | 3464 B（5.3%） |
| SRAM（RW + ZI） | 18072 B | 20480 B | 2408 B（11.8%） |

> 中文字库 `oledfont_cn.h` 是 Flash 最大单项，已从 151 字裁到 73 字
> （点阵 + 码点表共 2482 B），省下的约 2.6 KB 正好支付了设备端改主密码向导
> （应用层 +1100 B）还净余 1.5 KB。想再腾 Flash 先跑
> `python tools/gen_oled_font.py --check --trim` 看有没有冗余，没有就该考虑
> 别的办法，**不要去砍安全逻辑**。

### 校验构建产物：别看 hex 的 MD5

`HARDWARE/ds3231.c` 用 `__DATE__` / `__TIME__` 生成编译期 RTC 基准时间，
**每次构建的 hex 必然不同**（实测：源码一字未改连做两次 `-r`，
`Program Size` 一模一样而 hex 的 MD5 不同）。判断"到底有没有重编"要看：

1. 构建日志里有没有**全部** `compiling *.c...` 行（`-r` 全量重建应有此行）；
2. 日志里有没有 `Program Size:` 行（没有 = 根本没链接）。

另外 **UV4 会偶发启动失败**：`Start-Process -Wait -PassThru` 拿回空退出码、
`-o` 指定的日志文件压根没生成、`obj/` 时间戳不动。看到这三种症状就是"没跑
起来"，直接重试（`try/catch` + 最多 3 次），千万别当成构建成功。

参考工程同芯片的对应值是 `Code=53076 RO-data=7848 RW-data=484 ZI-data=19844`，
即它已用掉 20328/20480 B 的 SRAM —— 余量只有 152 B。本工程代码量与它基本
持平，但**任务栈与队列全部走静态分配**，因此不能再保留参考工程那样的
16 KB 动态堆，否则 armlink 直接报：

```
Error: L6406E: No space in execution regions with .ANY selector matching heap_4.o(.bss).
Error: L6407E: Sections of aggregate size 0x3674 bytes could not fit into .ANY selector(s).
```

`configTOTAL_HEAP_SIZE` 现为 **4 KB**，这块堆只服务三个瞬时调用方：
cJSON 解析（`app_wifi.c` 的钩子）、MqttKit 报文缓冲、`esp8266.c` 下行解包。
应用层自身不使用动态分配。

SRAM 的其余占用（供排查参考）：

| 组成 | 字节 |
|---|---:|
| FreeRTOS 动态堆 `ucHeap` | 4096 |
| 5 个应用任务静态栈（256/384/320/256/160 字） | 5504 |
| 空闲任务 + 定时器任务栈与控制块 | 1688 |
| 启动文件 MSP 栈 | 1024 |
| USART1/2/3 接收环形缓冲（512/256/128） | 908 |
| esp8266 线路/下行缓冲 | 680 |
| `board_port.c` 静态状态 | 576 |
| FreeRTOS 内核控制块（TCB/队列/堆） | 约 600 |

**上板后仍需实测**：用 `uxTaskGetStackHighWaterMark()` 校验每个任务的栈深度，
用 `xPortGetFreeHeapSize()` / `xPortGetMinimumEverFreeHeapSize()` 校验动态堆
峰值。若发现堆吃紧，优先关掉未使用的 FreeRTOS 软件定时器任务
（`configUSE_TIMERS = 0`，本工程当前没有任何 `xTimer*` 调用，可回收约 1.8 KB），
而不是压缩安全任务栈。

## 构建（GCC 编译校验）

仓库内可直接做**编译校验**（不链接）：

```powershell
cmake -S . -B build/arm-check -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake `
  -DSMART_LOCK_BUILD_TESTS=OFF `
  -DSMART_LOCK_BUILD_FIRMWARE_SYNTAX=ON
cmake --build build/arm-check
```

这一步会用 arm-none-eabi-gcc 把 FreeRTOS 内核 + GCC 移植层 + 全部应用/板级
源码，放到真实的 StdPeriph 头文件与参考工程驱动头文件下过一遍类型与签名检查。
`SMART_LOCK_REF_ROOT` 缓存变量指向参考工程位置，可用 `-D` 覆盖。

要产出可烧录镜像请用上面的 **Keil MDK 工程**：启动文件、散度加载脚本、
StdPeriph 与驱动 `.c` 都已登记在 `SmartLock.uvprojx` 里，`Rebuild` 即出
`obj/SmartLock.hex`。CMake 侧目前只做到编译校验、不链接固件。

## 引脚

以 `docs/pinout.md` 为准。要点：

- 键盘行 PA11/PA12/PA15/PB3、列 PB4/PB5/PB6/PB7（与参考工程 `key_4x4.c`
  一致，必须按键盘板丝印 `R1~R4`/`C1~C4` 接，否则上下镜像）；
  PA15/PB3/PB4 原属 JTAG，需 `sys_init()` 释放。
- RC522 的 RST = **PC14**；PC13 门磁与防撬本轮**未启用**。
- 软件 I2C 在 PB8/PB9；舵机 TIM3_CH3 = PB0；蜂鸣器 PC15；RGB = PA0/PA1/PB1。

## 键盘交互（含设备端改主密码）

4×4 键盘的按键语义在 `firmware/app/src/keypad_input.c` 里翻译成 `board_input_event_t`，
业务处理在 `smart_lock_app.c` 的 `handle_board_input()`：

| 键 | 语义 |
|---|---|
| `0`~`9` | 累积进输入缓冲（虚位密码允许前缀乱码，缓冲满 32 位按 FIFO 丢最旧） |
| `*` | 仅作首字符有效：进入 TOTP 动态口令模式；已有输入时按它当取消 |
| `#` | 确认，产出 `BOARD_AUTH_PIN`（或 `*` 模式下的 `BOARD_AUTH_TOTP`） |
| `A` / `B` | 查看最近一条审计记录 / 继续往前翻 |
| `C` | 取消：清空缓冲 + 复位日志游标，并产出 `BOARD_KEYPAD_CANCEL` |
| `D` | 进入管理向导（改主密码），产出 `BOARD_MENU_REQUEST` |

### 改主密码向导

原密码 `123456`（出厂值）→ 待机时按 **`D`** → 按提示走三步：

```
按 D  ──▶ 第4行: 原密码    ──输入原密码 + #──▶ 第4行: 新密码
                                                    │
       ┌────────────────输入新密码(6~12位) + #───────┘
       ▼
   第4行: 再输一遍 ──两遍一致 + #──▶ 落盘 W25Q64 配置 A/B 槽
                    └─不一致──▶ 退回"新密码"重输
按 C / 60 s 超时 ──▶ 退出向导，第4行还原"请输入密码或指纹"
```

设计要点（改动时请守住）：

- **第 4 行（提示行）归板级层独占**。向导的分步提示语用 `board_ui_set_prompt()`
  传 `board_prompt_t` 枚举，汉字常量集中在 `board_port.c` —— 整张中文字库是按
  `board_port.c` 的字符串字面量扫出来的，汉字一旦散到别的文件就会漏字、屏上
  变成空心方框。向导期间 `ui_render_status()` 也会把这一行让给向导，
  免得被并发的状态消息（"Access granted" 之类）顶掉。
- **原密码校验复用 `pin_auth_state`**（`keypad_is_locked` /
  `keypad_register_failure`）。若另开一套计数器，攻击者就能靠反复按 `D` 提交
  原密码，把"连续 3 次错误锁定 60 s"的策略绕过去。
- **先落盘、后报成功**：新密码标签先算在 `pin_credential_t` 副本上，
  `board_security_save()` 真的写进 W25Q64 才改内存并报 OK；写失败就回滚，
  不允许出现"屏幕说改好了、重启又变回旧密码"。
- **向导期间不进 STOP**，否则一睡下去键盘扫描就停了，用户会以为"改完密码门锁死了"。
- **超时 60 s 自动退出**（`PIN_WIZARD_TIMEOUT_SECONDS`），否则向导会永远挂在
  "待输入新密码"，下一个路过的人按 `#` 就把主密码设成他随手输的几位数了。
- 成功落盘会记一条 `LOCK_EVENT_CONFIG_CHANGE` 审计（日志界面显示"改密码"）。
  该枚举值**追加在 `lock_event_type_t` 末尾**——`event_type` 在 W25Q64 上按
  `uint8_t` 持久化，插在中间会把历史记录整体错位。

> 指纹库（AS608）与 W25Q64 是两套独立存储：**擦 W25Q64 不会清掉已录的指纹**，
> 旧指纹仍然能开门。要清指纹得走 AS608 自己的删模板命令。

## 集成步骤

1. 把 `firmware/app/include`、`firmware/stm32/include`、`firmware/FreeRTOS/include`、
   参考工程的 `HARDWARE`/`MIDDLEWARE`/`SYSTEM`/`USER` 加入包含路径。
2. 源文件：FreeRTOS 内核 + GCC 移植层 + `firmware/app/src/*.c` +
   `firmware/stm32/src/*.c` + 参考工程的 StdPeriph 与驱动 `.c`。
   注意 `firmware/stm32/src/stm32f10x_it.c` **不要**再定义
   SVC/PendSV/SysTick 三个 Handler（由 FreeRTOS 移植层接管）。
3. `board_port.c` 已实现 `board_port.h` 的全部接口。改动硬件时同步
   `smart_lock_pinmap.h` 与参考工程驱动里写死的引脚两处。
4. `main()` 在时钟/外设初始化之后调用 `smart_lock_app_start()`，再
   `vTaskStartScheduler()`。
5. 跑主机测试 → `arm-none-eabi-gcc` 编译 → ST-Link 烧录 → 执行
   `docs/bring-up.md` 的流程。

## W25Q64 分区

| 区域 | 地址 | 说明 |
|---|---|---|
| 配置 A 槽 | 0x000000 | 4096 B，版本号 + 序号 + CRC32 信封 |
| 配置 B 槽 | 0x001000 | 4096 B，A/B 交替写，掉电不丢 |
| 卡片表 | 0x002000 | 最多 32 张，UID 4 字节 + user_id |
| 审计日志环 | 尾部 128 扇区 | 每扇区一条记录，一次擦除 + 一次写入 |

审计日志刻意"一记录一扇区"：浪费容量，但掉电语义最容易验证。后续版本可在
补上掉电测试后改为打包记录 + 双扇区压缩。

## 发布前必做

- 逐台写入唯一的 32 字节设备密钥，**不得提交到版本库**。
- 只存 owner/visitor PIN 的 HMAC 摘要。
- 烧录并验证救援流程后，再打开读保护（RDP level 1）。
- 打开独立看门狗（`SMART_LOCK_ENABLE_IWDG = 1`）并确认喂狗点覆盖所有任务。
- RC522 UID 只作便捷因子，不作为高安全因子。
