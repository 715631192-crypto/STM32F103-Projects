# -*- coding: utf-8 -*-
"""
Generate docs/智能门锁项目文档案例.docx
参照 "基于STM32的环境监测调节系统" 项目文档案例 的格式：
  标题 → 硬件及连接(表格) → 主要开发工具 → 功能介绍(要点) → 系统运行逻辑(编号子节+公式/代码)
内容来自本工程（STM32 金融级安全物联网智能门锁）的真实设计。
"""
import os
from docx import Document
from docx.shared import Pt, RGBColor, Inches
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn
from docx.oxml import OxmlElement

OUT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "docs",
                                    "智能门锁项目文档案例.docx"))

doc = Document()

# ---------- base style ----------
normal = doc.styles["Normal"]
normal.font.name = "Microsoft YaHei"
normal.font.size = Pt(11)
# ensure east-asian font
rpr = normal.element.get_or_add_rPr()
rfonts = rpr.find(qn("w:rFonts"))
if rfonts is None:
    rfonts = OxmlElement("w:rFonts")
    rpr.append(rfonts)
rfonts.set(qn("w:eastAsia"), "Microsoft YaHei")
rfonts.set(qn("w:ascii"), "Microsoft YaHei")
rfonts.set(qn("w:hAnsi"), "Microsoft YaHei")

def set_cjk(run, font="Microsoft YaHei"):
    run.font.name = font
    r = run._element.get_or_add_rPr()
    rf = r.find(qn("w:rFonts"))
    if rf is None:
        rf = OxmlElement("w:rFonts"); r.append(rf)
    rf.set(qn("w:eastAsia"), font)
    rf.set(qn("w:ascii"), font)
    rf.set(qn("w:hAnsi"), font)

def shade(cell, hexcolor):
    tcPr = cell._tc.get_or_add_tcPr()
    sh = OxmlElement("w:shd")
    sh.set(qn("w:val"), "clear")
    sh.set(qn("w:fill"), hexcolor)
    tcPr.append(sh)

def title(text):
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    r = p.add_run(text)
    r.bold = True; r.font.size = Pt(20)
    set_cjk(r)
    # bottom border
    pPr = p._p.get_or_add_pPr()
    pbdr = OxmlElement("w:pBdr")
    bottom = OxmlElement("w:bottom")
    bottom.set(qn("w:val"), "single"); bottom.set(qn("w:sz"), "6")
    bottom.set(qn("w:space"), "4"); bottom.set(qn("w:color"), "1F6FB2")
    pbdr.append(bottom); pPr.append(pbdr)
    doc.add_paragraph()

def h1(text):
    p = doc.add_paragraph()
    r = p.add_run(text); r.bold = True; r.font.size = Pt(15)
    r.font.color.rgb = RGBColor(0x1F, 0x6F, 0xB2)
    set_cjk(r)
    return p

def h2(text):
    p = doc.add_paragraph()
    r = p.add_run(text); r.bold = True; r.font.size = Pt(12.5)
    set_cjk(r)
    return p

def body(text, size=11):
    p = doc.add_paragraph()
    r = p.add_run(text); r.font.size = Pt(size); set_cjk(r)
    return p

def bullet(text, size=11):
    p = doc.add_paragraph(style="List Bullet")
    r = p.add_run(text); r.font.size = Pt(size); set_cjk(r)
    return p

def code_block(lines, size=10):
    # shaded monospace block
    tbl = doc.add_table(rows=1, cols=1)
    tbl.alignment = WD_TABLE_ALIGNMENT.LEFT
    cell = tbl.cell(0, 0)
    shade(cell, "F2F4F7")
    cell.width = Inches(6.6)
    tf = cell.paragraphs[0]
    tf.text = ""
    for i, line in enumerate(lines):
        if i > 0:
            tf.add_run().add_break()
        run = tf.add_run(line if line else " ")
        run.font.name = "Consolas"
        run.font.size = Pt(size)
        run.font.color.rgb = RGBColor(0x10, 0x2A, 0x43)
        # monospace needs ascii/hAnsi set; east-asia fallback
        rr = run._element.get_or_add_rPr()
        rf = rr.find(qn("w:rFonts"))
        if rf is None:
            rf = OxmlElement("w:rFonts"); rr.append(rf)
        rf.set(qn("w:ascii"), "Consolas"); rf.set(qn("w:hAnsi"), "Consolas")
        rf.set(qn("w:eastAsia"), "Microsoft YaHei")
    doc.add_paragraph()

# ============================================================
# 标题
# ============================================================
title("基于STM32的金融级安全物联网智能门锁")

# ============================================================
# 1. 硬件及连接
# ============================================================
h1("硬件及连接")
body("本系统以 STM32F103C8T6 为主控，外挂 WiFi、指纹、读卡、时钟、存储、显示、执行器与键盘等模块，"
     "通过 USART / SPI / I2C / 定时器 PWM 与 GPIO 互联。引脚分配如下：")
hw = [
    ("硬件名称", "数量", "引脚"),
    ("STM32F103C8T6 最小系统板", "1", "Cortex-M3 @72MHz，主控"),
    ("ESP-12F WiFi 模块 (ESP8266)", "1", "USART1：PA9( TX / AF_PP )、PA10( RX / 浮空 )；EN→3V3、GPIO0→GND 进下载"),
    ("AS608 指纹模块", "1", "USART2：PA2( TX )、PA3( RX )；指纹存模块内部 Flash"),
    ("RC522 RFID 读卡模块", "1", "SPI1：PA4~PA7 ( NSS/SCK/MISO/MOSI )；RST=PC14；13.56MHz Mifare"),
    ("W25Q64 8MB Flash", "1", "SPI2：PB12~PB15 ( CS/SCK/MISO/MOSI )；配置/日志/卡表"),
    ("0.96寸 OLED (SSD1306, I2C)", "1", "PB8( SCL )、PB9( SDA )"),
    ("DS3231 高精度时钟", "1", "PB8/PB9 ( I2C，地址 0xD0 )；VBAT 纽扣电池断电保持"),
    ("舵机 (锁体执行器)", "1", "TIM3_CH3：PB0"),
    ("蜂鸣器 (低电平触发)", "1", "PC15"),
    ("RGB 状态指示灯", "1", "红 PA0 / 绿 PA1 / 蓝 PB1"),
    ("4×4 矩阵键盘", "1", "行 PA11/PA12/PA15，列 PB4~PB7（PA15/PB3/PB4 需释放 JTAG）"),
    ("门磁 / 唤醒键", "1", "PA8（EXTI8 唤醒）"),
    ("调试串口", "—", "USART3：PB10/PB11 @115200 8N1"),
]
t = doc.add_table(rows=len(hw), cols=3)
t.style = "Table Grid"
t.alignment = WD_TABLE_ALIGNMENT.CENTER
widths = [Inches(2.3), Inches(0.7), Inches(3.6)]
for ri, row in enumerate(hw):
    for ci, val in enumerate(row):
        c = t.cell(ri, ci)
        c.width = widths[ci]
        c.paragraphs[0].text = ""
        r = c.paragraphs[0].add_run(val)
        r.font.size = Pt(9.5)
        set_cjk(r)
        if ri == 0:
            r.bold = True
            shade(c, "1F6FB2")
            r.font.color.rgb = RGBColor(0xFF, 0xFF, 0xFF)
        else:
            shade(c, "FFFFFF" if ri % 2 else "EEF3F8")
doc.add_paragraph()

# ============================================================
# 2. 主要开发工具
# ============================================================
h1("主要开发工具")
bullet("Keil uVision5（ARMCC AC5 V5.06u7）：工程编译与无头构建（UV4 -r 批处理）。")
bullet("STM32CubeProgrammer：SWD 烧录与校验（STM32_Programmer_CLI.exe -c port=SWD -w SmartLock.hex -v -rst）。")
bullet("串口调试助手（如 SSCOM）：COM7 @115200 查看 printf 调试日志。")
bullet("微信小程序 / APP：经 MQTT 下行远程开锁指令（与 OneNET 或自建 Broker 对接）。")
bullet("逻辑分析仪 / 示波器：定位 I2C 时序、SPI 波形与电源跌落等硬件问题。")

# ============================================================
# 3. 功能介绍
# ============================================================
h1("功能介绍")
bullet("多因子认证开锁：6 位密码 / 指纹 / IC 卡 / TOTP 动态口令（兼容 Google Authenticator）/ APP 一次性临时密码。")
bullet("胁迫机制：在正确密码前后加任意数字即静默开门，并在后台记录“胁迫事件”（不惊动现场）。")
bullet("双因子组合：可在菜单设置“指纹+密码”或“卡+密码”，先验证第一因子再输 PIN 才放行。")
bullet("远程开锁：ESP8266 经 MQTT 接收云端下行指令，由 network_task 注入 remote_queue 完成开锁。")
bullet("自动落锁：开锁后按 AUTO_LOCK_DELAY_SECONDS 定时落锁；门关闭（门磁）立即落锁；超时未关门也兜底落锁。")
bullet("防暴力破解：连续 3 次失败锁定 60 秒；管理菜单入口复用同一失败计数，防止连按 'D' 绕过。")
bullet("审计日志：每次开锁 / 失败 / 胁迫 / 防拆写入 W25Q64，CRC32 校验，断电不丢。")
bullet("防拆报警：检测到撬壳 / 断线进入 ALARM 状态，拉蜂鸣器并 RGB 红闪。")
bullet("掉电安全存储：配置用 A/B 双槽 + 序号 + CRC32 信封，写一半断电不损坏。")
bullet("高可靠读卡：RC522 两级自愈（配置回读 + 周期重初始化），用久不失灵。")
bullet("显示稳定：OLED 与 DS3231 共用 I2C，事务级互斥锁，屏幕不乱码。")

# ============================================================
# 4. 系统运行逻辑
# ============================================================
h1("系统运行逻辑")

h2("4.1 系统启动流程")
body("固件入口极薄：main() 仅调用 smart_lock_app_start()，成功后再起 FreeRTOS 调度器；"
     "失败则进入死循环等待看门狗复位。smart_lock_app_start() 负责初始化、创建 6 个队列与 5 个任务，"
     "其中 board_init() 按以下 9 步完成板级初始化：")
code_block([
    "1) 中断分组 + 释放 PA15/PB3/PB4（否则键盘不可用）",
    "2) 三路串口：USART1 ESP8266 / USART2 指纹 / USART3 调试",
    "3) IIC_Init + OLED_Init + OLED_Clear（不清屏→随机噪点带）；蜂鸣器/RGB",
    "4) W25Q64_Init（日志与配置存储，失败即返回 false）",
    "5) RC522：MISO 探针→Init→Check 不通过再 Init 一次（救回首次复位）",
    "6) DS3231_Init（优先真实模块，无 ACK 自动回落软时钟）",
    "7) 舵机 init + 上电回到锁闭位",
    "8) 键盘/唤醒键 init + 卡表加载",
    "9) app_wifi_init（注册 MQTT 下行回调） + watchdog_init",
], size=9.5)

h2("4.2 FreeRTOS 任务与队列")
body("系统采用抢占式多任务，全部静态分配（configTOTAL_HEAP_SIZE=4KB）。5 个任务通过 6 个队列解耦，"
     "输入→决策→执行全异步，认证主线程不阻塞：")
code_block([
    "任务           优先级   栈(字)   职责",
    "access_task     4       384      认证主线程：处理输入/菜单/锁控",
    "input_task      3       256      扫键盘/卡/指纹 → board_input_event",
    "network_task    2       768      ESP8266 MQTT 上行/下行、远程开锁",
    "log_task        1       256      消费 log_queue 写 W25Q64 环形日志",
    "ui_task         1       160      刷 OLED（仅被 ui_queue 唤醒）",
    "",
    "队列：input / remote / log / notify / ui / log_request（均 xQueueCreateStatic）",
], size=9.5)

h2("4.3 认证状态机（lock_controller）")
body("锁控逻辑用 4 态有限状态机解耦“决策”与“执行”：LOCKED（默认锁闭）、UNLOCKED（授权开锁）、"
     "LOCKOUT（失败锁定）、ALARM（防拆报警，最高优先级）。状态转移规则如下：")
code_block([
    "LOCKED  --AUTH_GRANTED/REMOTE_UNLOCK-->  UNLOCKED(unlock_deadline=now+自动落锁秒)",
    "UNLOCKED--门关闭 DOOR_CLOSED / TICK超时-->  LOCKED",
    "任意态  --连续失败达上限 LOCKOUT-->  LOCKOUT(lockout_deadline)",
    "LOCKOUT --时间到 CLEAR_LOCKOUT-->  LOCKED",
    "任意态  --TAMPER-->  ALARM  （只有 CLEAR_ALARM 能退出）",
    "",
    "set_physical_state() 是唯一真正动硬件的点：",
    "  board_lock_set(state != UNLOCKED);   // 非 UNLOCKED 都锁上",
    "  board_alarm_set(state == ALARM);     // 仅 ALARM 拉报警",
], size=9.5)

h2("4.4 凭据安全（HMAC + 常数时间比较 + 虚拟前后缀）")
body("密码不以明文存储，而以 HMAC(device_secret, PIN) 的摘要标签保存；校验时用常数时间比较，"
     "避免响应耗时泄露匹配进度（防时序侧信道）：")
code_block([
    "bool lock_constant_time_equal(const uint8_t *l, const uint8_t *r, size_t n){",
    "    uint8_t diff = 0;",
    "    for (size_t i=0;i<n;++i) diff |= (uint8_t)(l[i] ^ r[i]); // 永远比完",
    "    return diff == 0;               // 不等也耗相同时长",
    "}",
    "",
    "// 虚拟前后缀 = 胁迫码：正确 PIN 前后加任意数字也算对",
    "for (start=0; start<=len-stored_len; ++start)",
    "    if (子串匹配) return 匹配;   // 胁迫指纹/密码静默开 + 后台告警",
], size=9.5)

h2("4.5 动态口令 TOTP（RFC6238）")
body("动态口令兼容 Google Authenticator：计数器 counter = unix_time / 30（30 秒一步），"
     "用 HMAC-SHA1 做 HOTP 动态截断得到 6 位码；校验允许前后各一个时间窗抵消时钟抖动。")
code_block([
    "counter = unix_time / 30                  // 时间步 30s",
    "HMAC-SHA1(secret, counter大端8字节) → digest[20]",
    "offset = digest[19] & 0x0F               // 动态截断",
    "code = ((digest[offset]&0x7F)<<24 | ... ) % 10^digits   // 取低 6 位",
    "",
    "校验容窗：遍历 [now-30, now, now+30] 三个 counter，任一命中即放行",
], size=9.5)

h2("4.6 I2C 总线并发与互斥锁（屏不乱码）")
body("OLED(0x78) 与 DS3231(0xD0) 共用同一条软 I2C(PB8/PB9)。高优先级任务在“一次 I2C 事务传到一半”时"
     "抢断低优先级，START/数据/STOP 交织→屏幕花屏、文字错乱。修法：把锁下沉到“事务层”，"
     "任一 START..STOP 独占总线，且用递归锁（oled.c 会嵌套进入）：")
code_block([
    "static SemaphoreHandle_t s_i2c_mtx = NULL;   // 总线互斥锁(递归)",
    "static void i2c_lock(void){",
    "    if (s_i2c_mtx && xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED)",
    "        xSemaphoreTakeRecursive(s_i2c_mtx, portMAX_DELAY);",
    "}",
    "// IIC_WriteBytesRaw / IIC_ReadRegs / IIC_WriteReg 均在事务首尾加锁",
], size=9.5)

h2("4.7 RC522 读卡自愈（不失灵）")
body("ID 卡用一阵后失灵的根因：旧逻辑用 VersionReg(0x37)（固定硅片 ID）判断“芯片活着”——但芯片即便"
     "掉电复位、配置全丢，该 ID 照样读出，于是只做 AntennaOn 永远补不回配置。正确判据是回读 Init 写过的"
     " TModeReg(0x8D)/TPrescalerReg(0x3E)。连续无卡时两级自愈：")
code_block([
    "static void card_reader_heal(void){",
    "    bool ok = (MFRC522_Check() != 0U);          // 回读配置寄存器",
    "    if ((!ok) || (++s_heal >= CARD_REINIT_AFTER_HEAL)){",
    "        s_heal = 0; MFRC522_Init();            // ①配置丢 或 ②周期刷新",
    "    } else MFRC522_AntennaOn();                 // 天线位被清则补开",
    "}",
    "// 触发：poll_card 连续 50 轮(≈5s)无卡才自愈；场上无卡才做(~13ms,安全)",
], size=9.5)

h2("4.8 存储与审计（config_store + event_log）")
body("配置用带版本+序号的 CRC32 信封、A/B 双槽掉电安全写入；写永远写“非活动槽”，擦/写中途断电旧槽仍完好。"
     "审计日志每条带 CRC32，挡住写一半的坏记录，序号用 int32 环绕比较保证 newest 正确：")
code_block([
    "信封(小端)：magic 'SLC1'=0x31434C53 | version | payload_len | sequence | payload | crc32",
    "读：两槽都解析，取 CRC 正确且 sequence 较大者",
    "写：写非活动槽(先擦后写)，sequence = 活动序号 + 1",
    "",
    "crc32：标准 IEEE 多项式 0xEDB88320；record 有效 = magic 对 且 crc 对",
    "sequence_is_newer(a,b): (int32_t)(a-b) > 0   // 正确处理序号回绕",
], size=9.5)

doc.save(OUT)
print("SAVED", OUT, os.path.getsize(OUT), "bytes")
