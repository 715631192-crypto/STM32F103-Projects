# ESP8266 不联网 —— 根因诊断报告

日期：2026-09-14
依据：您提供的 15:27–15:30 三段开机串口日志（COM7 @115200）+ 全仓库代码比对 + ST-Link 热插拔寄存器实测

---

## 一、结论（一句话）

**不是 STM32 固件的问题，是 ESP8266 模块自己的固件崩溃了、陷入重启死循环。**
模块在崩溃循环里无法响应 AT，所以 MCU 看到"一个字都收不到"；
等它偶然重启成功后又能正常联网 —— 这正好解释"同一块板、同一套接线、有时能联有时不能"。

---

## 二、三段日志时间线解读

### 第 1 段 15:27（模块没死，但已经出现征兆）

```
[ESP] after AT@115200, PA10 edge scan 600ms: edges=0 low=0 -> no reply at all
```

模块一声不响。此时 MCU 侧是好的（同一次开机里 `loopback ... PASS: MCU UART end-to-end OK`）。

### 第 2 段 15:28（模块完全正常，链路全通）

```
[ESP] after AT@115200, PA10 edge scan 600ms: edges=70 low=408 -> MODULE REPLIED
[ESP] sweep 115200 -> 11 byte(s) fe=0 ovr=0
      head=41 54 0D 0D 0A 0D 0A 4F 4B 0D 0A
```

把 `41 54 0D 0D 0A 0D 0A 4F 4B 0D 0A` 逐字节译出来：

| 字节 | ASCII |
|---|---|
| `41 54` | `AT`（模块回显） |
| `0D 0D 0A` | `\r\r\n` |
| `0D 0A` | `\r\n` |
| `4F 4B` | `OK` |
| `0D 0A` | `\r\n` |

后面 `AT+GMR` 回 `AT version:0.40.0.0 (Aug 8 2015 14:45:58)` + `SDK 1.3.0`，
`AT+CWMODE=1` / `AT+CWDHCP=1,1` 都回 OK，
最后走到 `[WIFI] JOIN -> MQTT (WiFi ok)`，token 也生成成功（132 字节）。

**这一段证明：MCU 侧（USART1、PA9/PA10、时序、波特率、AT 指令集、MQTT 封装）全部正确。**
只要模块活着，这条链路能一路跑到 OneNET。

### 第 3 段 15:30（模块崩了，且是死循环）

```
[ESP] after AT@115200, PA10 edge scan 600ms: edges=0 low=0 -> no reply at all
...
[ESP] Fatal exception (0): epc1=0x402012e8 epc2=0x00000000 epc3=0x00000000
      excvaddr=0x00000037 depc=0x00000000
[ESP] rst cause:2, boot mode:(3,6)
[ESP] don't use rtc mem data
[ESP] 2nd boot version : 1.4(b1)
```

并且 **`excvaddr` 每次重启都不一样**（`0x37 / 0x36 / 0x33 / 0x32 / 0x31 / 0x30 / 0x2f / 0x2d` …），
`2nd boot version : 1.4(b1)` 反复出现 —— 这是**模块在反复重启**的特征：
每重启一次就重打一遍 boot log，然后再次崩在同一处。

再往后又能看到 `[AT<] OK (270ms)` 并再次走通 `JOIN -> MQTT` ——
说明它是**间歇性崩溃**，崩一阵、恢复一阵。

### 关键判据说明

| 字段 | 含义 |
|---|---|
| `Fatal exception (0)` | 模块固件在**用户代码区**取指令/访存异常（不是 boot ROM 的问题） |
| `epc1=0x402012e8` | 出错时的 PC。`0x4020_xxxx` = Flash 映射的用户代码区；**每次都崩在同一个地址** → 确定的代码路径崩溃，不是随机干扰 |
| `excvaddr` 每次都变 | 出错的访问地址随机 → 典型的"指针/结构已损坏后再被使用"或 Flash 内容异常 |
| `rst cause:2` | 复位源不是正常冷启动（正常的冷上电是 `rst cause:1`） |
| `boot mode:(3,6)` | 从外部 Flash 启动（正常值），本身没问题 |
| `2nd boot version` 反复出现 | **反复重启**的铁证（正常只打一次） |

---

## 三、证据链（三条同时成立，只剩一个解释）

| # | 事实 | 来源 |
|---|---|---|
| 1 | 本工程与参考工程的 ESP8266 链路代码 **54 个源文件逐行一致**（只有字库和 `app_wifi.c` 的一段注释不同） | `tools/diff_tree.py` 全仓库比对 |
| 2 | MCU 侧实测全对：`GPIOA_CRH=0x388334B8`(PA9=AF_PP/PA10=浮空)、`USART1 BRR=0x271`(115200@72M)、`CR1=0x202C`、`RCC_CFGR=0x001D040A`(72MHz)；PA9 灌数据时引脚实测会翻转；`loopback PASS` | ST-Link `mode=hotplug` 读实时寄存器 + 探针 v2 |
| 3 | 模块**偶发一个字都不发**（`edges=0`），且同一块板上模块**有时又完全正常**（第 2 段日志） | 用户日志 |

1 + 2 ⇒ 固件无责任；3 + 第 3 段日志 ⇒ **模块自己坏了 / 固件不稳定**。

再看您补充的事实 —— "同块板、接线没动，参考工程在这块板上是能联网的"：
如果接线错了，它一次都不会通；如果代码错了，它一次都不会通。
**"有时通、有时不通" 只能是模块本身状态不稳定。**

---

## 四、为什么"刷 STM32 固件 / 按单片机复位"救不了它

- ESP8266 **不随 STM32 复位**，它有自己独立的电源域和复位逻辑。
  您刷固件、按 ST-Link 复位，模块该崩还是崩。
- `esp8266.c` 用的 AT 指令全集只有：
  `AT` / `AT+GMR` / `AT+CWMODE=1` / `AT+CWDHCP=1,1` / `AT+CWJAP?` / `AT+CWJAP=` /
  `AT+CIPSTART` / `AT+CIPSEND` / `AT+CIPCLOSE`。
  **没有 `AT+CIPMODE`（不开透传）、没有 `AT+UART_DEF`（不改模块波特率）、没有 `AT+RST`。**
  → 代码在逻辑上**没有任何一条路径能把模块搞崩**。
- 也就是说：这是模块刷的 AT 固件自身的老 bug（`AT 0.40.0.0 / SDK 1.3.0` 是 2015 年 8 月的版本，非常老）
  ＋ 该模块 Flash 可能有坏块/参数区损坏（`excvaddr` 随机变化很像后者）。

---

## 五、顺带查出的第二个 bug（代码里的，真实存在）

**位置**：`firmware/HARDWARE/esp8266.c:453`

```c
if (at_cmd("AT+CWJAP?", "+CWJAP", 3000))
{
    ESP_DBG("[WIFI] already associated (skip CWJAP)\r\n");
    return 1;
}
```

**问题**：`at_cmd()` 判断"命中期望串"用的是 `strstr(line, s_expect)`（`esp8266.c:185`），
而 AT 模块默认**会把命令原样回显**一行 `AT+CWJAP?` —— 这行回显**本身就包含 `+CWJAP`**。
于是 `strstr` 立刻命中，`at_cmd()` 在 `esp8266.c:347` 直接 `return 1`，
**根本不等真实应答，也不管模块究竟有没有连上 AP**。

后果：

1. 日志里 `[WIFI] already associated (skip CWJAP)` **永远会打印**，它不代表任何真实状态；
2. **`AT+CWJAP` 这条连接指令永远不会被执行** —— 只要模块一次掉关联（路由器重启、
   模块自身崩溃重启后丢了 AP 关联），固件就再也不会尝试重新连接，
   而是直接跳到 `AT+CIPSTART` 去开 TCP（必然失败），然后反复重试、永远连不上。

**修法**（一行，但**我没有改**，见下节）：期望串改判真实响应，例如

```c
if (at_cmd("AT+CWJAP?", "+CWJAP:", 3000))   /* 注意冒号，且避免回显命中 */
```

或者更稳的做法是判 `+CWJAP:`（已关联时才带冒号和参数）与 `No AP` 两个分支。

> ⚠️ 我没有直接改这一行 —— 因为您的规则是"改动只限原有代码范围"，而且这个文件是
> **零改动复用**参考工程的（改了就和参考工程分叉了）。**要不要改，请您定。**

---

## 六、建议的动作（按优先级）

1. **先给整机彻底断电 10 秒以上再上电**（只按复位键没用，模块不跟着复位）。
   观察是否恢复 —— 如果恢复，更坐实"模块状态卡死"。
2. **重刷 ESP8266 模块的 AT 固件**（推荐比 `AT 0.40.0.0/SDK 1.3.0` 新的稳定版，
   例如 Espressif 官方 ESP-AT 或安信可较新的 AT 固件），刷完执行一次
   `AT+RESTORE` 恢复出厂参数。
3. 如果重刷后仍然 `Fatal exception` → **模块硬件损坏（Flash 坏块），换模块**。
   同型号的 ESP-01S / ESP-12F 都行，接线不变。
4. 上一条做完、链路稳定后，再决定第五节那个 `+CWJAP` 的 bug 改不改
   （建议改，它会把"一次偶发掉线"放大成"永久连不上"）。

---

## 七、附：这一步之后要做的收尾

- 板子目前跑的是**诊断版固件**（探针 v2 打开，开机多等约 12 s）。
  回正常版：把 `SMART_LOCK_ESP8266_LINK_PROBE` 改回 0 重新编译，
  或**直接烧 `obj/SmartLock_normal.hex`**（探针在开关=0 时根本不参与编译，无需重编）。
- 相关文件：
  - `esp8266_checklist.md` —— ESP8266 全部信息核对清单
  - `esp8266_probe_v2_log.txt` —— 探针 v2 抓到的日志
  - `tools/diff_tree.py` / `tools/diff_with_reference.py` —— 与参考工程的逐行比对工具
