# -*- coding: utf-8 -*-
"""
Generate docs/code_walkthrough.pptx
A >=40 min project code walkthrough deck for the STM32 smart-lock firmware.
Every major function is shown with its real implementation (source code).
"""
import os
from pptx import Presentation
from pptx.util import Inches, Pt, Emu
from pptx.dml.color import RGBColor
from pptx.enum.text import PP_ALIGN, MSO_ANCHOR, MSO_AUTO_SIZE
from pptx.enum.shapes import MSO_SHAPE
from pptx.oxml.ns import qn

# ---------------- palette ----------------
BG          = RGBColor(0xF5, 0xF7, 0xFA)
DARK        = RGBColor(0x12, 0x21, 0x33)
ACCENT      = RGBColor(0x1F, 0x6F, 0xB2)   # blue
ACCENT2     = RGBColor(0x2E, 0x8B, 0x57)   # green
WARN        = RGBColor(0xC0, 0x39, 0x2B)   # red
CODE_BG     = RGBColor(0x0F, 0x1B, 0x2B)
CODE_FG     = RGBColor(0xE6, 0xED, 0xF3)
HEAD_FG     = RGBColor(0xFF, 0xFF, 0xFF)
SUB         = RGBColor(0x5A, 0x6B, 0x7B)
CARD        = RGBColor(0xFF, 0xFF, 0xFF)
CODECOMMENT = RGBColor(0x8B, 0x9B, 0xA8)
CODEKEY     = RGBColor(0x79, 0xC0, 0xFF)
CODESTR     = RGBColor(0x9E, 0xCE, 0x6A)

FONT_UI   = "Microsoft YaHei"
FONT_CODE = "Consolas"

PPTX_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         "..", "docs", "code_walkthrough.pptx")

prs = Presentation()
prs.slide_width  = Inches(13.333)
prs.slide_height = Inches(7.5)
SW, SH = prs.slide_width, prs.slide_height
BLANK = prs.slide_layouts[6]

# ---------------- helpers ----------------
def _set_run_font(run, name):
    """Set latin + east-asian + cs typeface so Chinese renders in code boxes."""
    run.font.name = name
    rPr = run._r.get_or_add_rPr()
    for tag in ("a:latin", "a:ea", "a:cs"):
        el = rPr.find(qn(tag))
        if el is None:
            el = rPr.makeelement(qn(tag), {})
            rPr.append(el)
        el.set("typeface", name)

def add_slide():
    return prs.slides.add_slide(BLANK)

def bg(slide, color=BG):
    s = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, 0, 0, SW, SH)
    s.fill.solid(); s.fill.fore_color.rgb = color
    s.line.fill.background()
    s.shadow.inherit = False
    # send to back
    slide.shapes._spTree.remove(s._element)
    slide.shapes._spTree.insert(2, s._element)
    return s

def rect(slide, x, y, w, h, color, line=None, shadow=False):
    s = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, x, y, w, h)
    s.fill.solid(); s.fill.fore_color.rgb = color
    if line is None:
        s.line.fill.background()
    else:
        s.line.color.rgb = line; s.line.width = Pt(1)
    if not shadow:
        s.shadow.inherit = False
    return s

def textbox(slide, x, y, w, h, anchor=MSO_ANCHOR.TOP):
    tb = slide.shapes.add_textbox(x, y, w, h)
    tf = tb.text_frame
    tf.word_wrap = True
    tf.vertical_anchor = anchor
    tf.margin_left = Pt(4); tf.margin_right = Pt(4)
    tf.margin_top = Pt(2); tf.margin_bottom = Pt(2)
    return tb, tf

def set_para(p, size, color, bold=False, align=PP_ALIGN.LEFT, font=FONT_UI,
             space_before=4, space_after=4, line=1.05):
    p.alignment = align
    p.space_before = Pt(space_before)
    p.space_after = Pt(space_after)
    try:
        p.line_spacing = line
    except Exception:
        pass
    for r in p.runs:
        r.font.size = Pt(size); r.font.bold = bold
        r.font.color.rgb = color; _set_run_font(r, font)
    return p

def add_para(tf, text, size, color, **kw):
    p = tf.add_paragraph()
    set_para(p, size, color, **kw)
    r = p.add_run(); r.text = text
    r.font.size = Pt(size); r.font.bold = kw.get("bold", False)
    r.font.color.rgb = color
    r.font.name = FONT_UI
    _set_run_font(r, kw.get("font", FONT_UI))
    return p, r

def title_bar(slide, kicker, title):
    rect(slide, 0, 0, SW, Inches(1.15),
         color=ACCENT if kicker != "安全" else WARN)
    tblk, tf = textbox(slide, Inches(0.5), Inches(0.12), SW-Inches(1), Inches(0.95),
                       anchor=MSO_ANCHOR.MIDDLE)
    add_para(tf, kicker, 12, RGBColor(0xCF,0xE6,0xFF) if kicker!="安全" else RGBColor(0xFF,0xD8,0xD8),
             bold=True, font=FONT_UI, space_after=2)
    add_para(tf, title, 26, HEAD_FG, bold=True, font=FONT_UI, space_after=0)

def footer(slide, idx, total, note=""):
    rect(slide, 0, SH-Inches(0.34), SW, Inches(0.34), color=DARK)
    tb, tf = textbox(slide, Inches(0.4), SH-Inches(0.34), SW-Inches(0.8), Inches(0.34),
                     anchor=MSO_ANCHOR.MIDDLE)
    add_para(tf, "STM32 智能门锁固件 · 代码讲解", 9, RGBColor(0xB8,0xC4,0xD0), font=FONT_UI,
             space_after=0)
    tb2, tf2 = textbox(slide, SW-Inches(3.0), SH-Inches(0.34), Inches(2.6), Inches(0.34),
                      anchor=MSO_ANCHOR.MIDDLE)
    add_para(tf2, "%d / %d   %s" % (idx, total, note), 9, RGBColor(0xB8,0xC4,0xD0),
             align=PP_ALIGN.RIGHT, font=FONT_UI, space_after=0)

def code_box(slide, x, y, w, h, code, size=11.0, line_spacing=1.04):
    rect(slide, x-Pt(2), y-Pt(2), w+Pt(4), h+Pt(4), color=RGBColor(0x22,0x33,0x48))
    tb, tf = textbox(slide, x, y, w, h, anchor=MSO_ANCHOR.TOP)
    tf.word_wrap = True
    # 自动缩放：超长函数整体缩小以适配文本框，绝不出框
    tf.auto_size = MSO_AUTO_SIZE.TEXT_TO_FIT_SHAPE
    first = True
    for raw in code.split("\n"):
        p = tf.paragraphs[0] if first else tf.add_paragraph()
        first = False
        p.alignment = PP_ALIGN.LEFT
        p.space_before = Pt(0); p.space_after = Pt(0)
        try: p.line_spacing = line_spacing
        except Exception: pass
        run = p.add_run(); run.text = raw if raw else " "
        run.font.size = Pt(size); run.font.name = FONT_CODE
        run.font.color.rgb = CODE_FG
        _set_run_font(run, FONT_CODE)
    return tb

def bullets(slide, x, y, w, h, items, size=14, color=DARK, gap=6):
    tb, tf = textbox(slide, x, y, w, h)
    for it in items:
        if isinstance(it, tuple):
            txt, lvl, c, b = it
        else:
            txt, lvl, c, b = it, 0, color, False
        p = tf.add_paragraph()
        p.level = lvl
        p.space_before = Pt(gap); p.space_after = Pt(0)
        try: p.line_spacing = 1.08
        except Exception: pass
        prefix = ("• " if lvl == 0 else "– ")
        run = p.add_run(); run.text = prefix + txt
        run.font.size = Pt(size); run.font.color.rgb = c
        run.font.bold = b; run.font.name = FONT_UI
        _set_run_font(run, FONT_UI)
    return tb

def callout(slide, x, y, w, h, text, color=ACCENT2):
    rect(slide, x, y, w, h, color=RGBColor(0xEC,0xF6,0xF0) if color==ACCENT2 else RGBColor(0xFB,0xEC,0xEA))
    bar = rect(slide, x, y, Pt(5), h, color=color)
    tb, tf = textbox(slide, x+Pt(10), y+Pt(4), w-Pt(16), h-Pt(8), anchor=MSO_ANCHOR.MIDDLE)
    add_para(tf, text, 12.5, DARK, font=FONT_UI, space_after=0)
    return tb

TOTAL = 42
def new_content(kicker, title, idx):
    s = add_slide()
    bg(s, BG)
    title_bar(s, kicker, title)
    return s

# =====================================================================
# 1. TITLE
# =====================================================================
s = add_slide()
bg(s, DARK)
rect(s, 0, Inches(2.5), SW, Inches(2.4), color=ACCENT)
tb, tf = textbox(s, Inches(0.9), Inches(2.6), SW-Inches(1.8), Inches(2.2), anchor=MSO_ANCHOR.MIDDLE)
add_para(tf, "STM32 金融级安全物联网智能门锁", 34, HEAD_FG, bold=True, space_after=8, font=FONT_UI)
add_para(tf, "固件代码讲解 · 源码级逐函数剖析", 20, RGBColor(0xCF,0xE6,0xFF), space_after=4, font=FONT_UI)
add_para(tf, "FreeRTOS · STM32F103C8 · 含全部函数实现（main / 任务 / 认证 / 锁控 / 存储 / 驱动）", 13,
         RGBColor(0xCF,0xE6,0xFF), space_after=0, font=FONT_UI)
tb2, tf2 = textbox(s, Inches(0.9), Inches(5.2), SW-Inches(1.8), Inches(1.4))
add_para(tf2, "讲解时长：约 40–50 分钟（42 页，逐函数 + 源码）", 13, RGBColor(0x9F,0xB3,0xC8),
         space_after=4, font=FONT_UI)
add_para(tf2, "硬件平台：STM32F103C8 @72MHz · 8MB W25Q64 · DS3231 · RC522 · AS608 · ESP-12F",
         12, RGBColor(0x9F,0xB3,0xC8), space_after=0, font=FONT_UI)

# =====================================================================
# 2. AGENDA
# =====================================================================
s = new_content("目录", "本次讲解的 6 大模块", 2)
items = [
    ("① 启动与系统骨架：main() → smart_lock_app_start() → board_init() 9 步", 0, DARK, False),
    ("② 并发模型：FreeRTOS 5 个任务 + 6 个队列，输入如何变成开锁", 0, DARK, False),
    ("③ 认证核心：状态机 lock_controller_handle + 凭据比较（常数时间 / 虚拟前后缀）", 0, DARK, False),
    ("④ 动态口令 TOTP（RFC6238）：hotp_generate / totp_verify 完整实现", 0, DARK, False),
    ("⑤ 持久化与安全：config_store 掉电安全信封 / event_log CRC32 环形日志", 0, DARK, False),
    ("⑥ 驱动与排错：I2C 互斥锁（屏不乱码）/ RC522 自愈（读卡不掉）/ 烧录调试", 0, DARK, False),
]
bullets(s, Inches(0.7), Inches(1.5), SW-Inches(1.4), Inches(4.6), items, size=16, gap=14)
callout(s, Inches(0.7), Inches(6.2), SW-Inches(1.4), Inches(0.7),
        "讲解方式：先给调用关系，再贴真实源码（每行中文注释），最后讲设计取舍与踩坑。",
        color=ACCENT)

# =====================================================================
# 3. ARCHITECTURE
# =====================================================================
s = new_content("架构", "四层架构：应用逻辑与硬件解耦", 3)
layers = [
    ("① 应用逻辑层  firmware/app/  （纯 C，可在 PC 上做单元测试）", ACCENT),
    ("② 板级适配层  firmware/stm32/src/board_port.c · smart_lock_app.c", ACCENT2),
    ("③ 驱动 / 中间件  firmware/HARDWARE/ · MIDDLEWARE/（RC522/AS608/I2C/W25Q64/TOTP）", RGBColor(0x8E,0x44,0xAD)),
    ("④ RTOS 内核  FreeRTOS V10.5.1（抢占式，全部静态分配）", RGBColor(0xCB,0x8E,0x00)),
]
y = Inches(1.55)
for txt, col in layers:
    rect(s, Inches(0.7), y, SW-Inches(1.4), Inches(0.85), color=col)
    tb, tf = textbox(s, Inches(0.95), y, SW-Inches(1.9), Inches(0.85), anchor=MSO_ANCHOR.MIDDLE)
    add_para(tf, txt, 15, HEAD_FG, bold=True, font=FONT_UI, space_after=0)
    y = y + Inches(1.05)
callout(s, Inches(0.7), Inches(6.05), SW-Inches(1.4), Inches(0.8),
        "解耦价值：同一套认证/锁控逻辑不依赖具体 MCU；board_port 只认“按键/卡/指纹”，不认“凭据”。"
        "这就解释了为什么管理菜单、加密比较必须放在应用层。", color=ACCENT2)

# =====================================================================
# 4. main()
# =====================================================================
s = new_content("启动", "① main()：只做两件事", 4)
code = r'''/* firmware/stm32/src/main.c —— 固件入口 */
int main(void)
{
    if (!smart_lock_app_start()) {
        /* 启动失败不可恢复：停等独立看门狗复位（发布版 IWDG=1）
           调试版可直接在调试器看调用栈 */
        for (;;) { }
    }
    vTaskStartScheduler();          /* 启动 FreeRTOS 调度器 */
    /* 正常不会返回；只有堆不足导致调度器没起来才会走到这里 */
    for (;;) { }
}'''
code_box(s, Inches(0.7), Inches(1.5), SW-Inches(1.4), Inches(3.4), code, size=13)
callout(s, Inches(0.7), Inches(5.2), SW-Inches(1.4), Inches(1.4),
        "要点：72MHz 时钟由启动文件调用的 SystemInit() 完成，main 不再配时钟。"
        "main 极薄——所有初始化、任务创建都下沉到 smart_lock_app_start()，"
        "避免“两份初始化互相打架”。若该函数返回 false（Flash 无响应 / 配置非法 / 任务创建失败），"
        "进入死循环等待看门狗复位。", color=ACCENT)

# =====================================================================
# 5. smart_lock_app_start()
# =====================================================================
s = new_content("启动", "② smart_lock_app_start()：初始化 + 建队列 + 建任务", 5)
code = r'''bool smart_lock_app_start(void)
{
    const event_log_storage_t storage = { .context=NULL,
        .slot_count = EVENT_LOG_SLOT_COUNT,
        .read = board_log_read, .write = board_log_write,
        .erase = board_log_erase };

    if ((!board_init()) || (!board_security_load(&security_config)) ||
        (!security_config_valid(&security_config)) ||
        (!event_log_init(&event_log, &storage)))
        return false;                       /* 任一步失败 → 上层死循环 */

    /* 初始化运行态：失败计数清零 / 门状态 / 锁控器 / 物理落锁 */
    memset(&pin_auth_state, 0, sizeof(pin_auth_state));
    menu_reset(); door_opened_at = 0U;
    clear_pending_factor();
    last_activity_at = board_unix_time();
    lock_controller_init(&controller, AUTO_LOCK_DELAY_SECONDS);
    set_physical_state(controller.state);

    /* 6 个静态队列（xQueueCreateStatic：内存来自 .bss，不占堆） */
    input_queue   = xQueueCreateStatic(INPUT_QUEUE_LENGTH,  sizeof(board_input_event_t), ...);
    remote_queue  = xQueueCreateStatic(REMOTE_QUEUE_LENGTH, sizeof(remote_command_t), ...);
    log_queue     = xQueueCreateStatic(LOG_QUEUE_LENGTH,    sizeof(event_log_record_t), ...);
    notify_queue  = xQueueCreateStatic(NOTIFY_QUEUE_LENGTH, sizeof(board_notification_t), ...);
    ui_queue      = xQueueCreateStatic(UI_QUEUE_LENGTH,     sizeof(app_ui_message_t), ...);
    log_request_q = xQueueCreateStatic(LOG_REQUEST_QUEUE_LENGTH, sizeof(uint32_t), ...);
    if (任一队列为 NULL) return false;

    /* 5 个任务（xTaskCreateStatic：栈也是静态数组，configTOTAL_HEAP_SIZE 仅 4KB） */
    xTaskCreateStatic(input_task,  "input",  INPUT_TASK_STACK_WORDS,  NULL, 3U, ...);
    xTaskCreateStatic(access_task, "access", ACCESS_TASK_STACK_WORDS, NULL, 4U, ...);
    xTaskCreateStatic(network_task,"network",NETWORK_TASK_STACK_WORDS,NULL, 2U, ...);
    xTaskCreateStatic(log_task,    "log",    LOG_TASK_STACK_WORDS,    NULL, 1U, ...);
    xTaskCreateStatic(ui_task,     "ui",     UI_TASK_STACK_WORDS,     NULL, 1U, ...);
    queue_ui(controller.state, "System ready", true);
    return true;
}'''
code_box(s, Inches(0.7), Inches(1.5), SW-Inches(1.4), Inches(4.5), code, size=11.5)
callout(s, Inches(0.7), Inches(6.15), SW-Inches(1.4), Inches(0.85),
        "全部用 Static 版本：队列缓冲区与任务栈都来自全局数组，不向 4KB 堆申请，"
        "杜绝碎片与分配失败。任务优先级 access(4)>input(3)>network(2)>log/ui(1)。", color=ACCENT)

# =====================================================================
# 6. board_init() 9 steps
# =====================================================================
s = new_content("启动", "③ board_init() 9 步初始化（上半）", 6)
code = r'''bool board_init(void)
{
  /* 1) 中断分组 + 释放 PA15/PB3/PB4（否则键盘不可用） */
  sys_init();  delay_init();
  /* 2) 三路串口：USART1 ESP8266 / USART2 指纹 / USART3 调试 */
  usart3_init(DEBUG_UART_BAUD);
  usart1_init(ESP8266_UART_BAUD);
  usart2_init(AS608_UART_BAUD);
  /* 3) 显示与本地交互 */
  IIC_Init();  OLED_Init();  OLED_Clear();   /* 不清屏→随机噪点带 */
  buzzer_init();  rgb_init();
  /* 4) 存储：W25Q64（日志与配置都放这里） */
  if (W25Q64_Init() != 1U) { 诊断上报; return false; }
  /* 5) 射频读卡：MISO 探针→Init→Check 不通过再 Init 一次
        （MCU 复位后 RC522 首次常不成功，二次硬复位能救回） */
  MFRC522_ProbeMiso();
  MFRC522_Init();
  if (!MFRC522_Check()) MFRC522_Init();'''
code_box(s, Inches(0.7), Inches(1.5), SW-Inches(1.4), Inches(4.0), code, size=12)
callout(s, Inches(0.7), Inches(5.7), SW-Inches(1.4), Inches(1.3),
        "顺序讲究：I2C 先 Init 才能创建总线互斥锁（见后）；OLED_Clear 必须在 Init 后立刻做，"
        "否则屏上残留随机噪点带（这是“乱屏”的第二个来源）；RC522 二次初始化是踩坑经验，"
        "少了它上板现象就是“卡贴上去没反应”。", color=ACCENT)

# =====================================================================
# 7. board_init() 9 steps (lower)
# =====================================================================
s = new_content("启动", "③ board_init() 9 步初始化（下半）", 7)
code = r'''  /* 6) 时钟：优先真实 DS3231（芯片内存 UTC），无 ACK 自动回落软时钟 */
  const uint8_t rtc_ok = DS3231_Init();
  DS3231_Time now; DS3231_GetTime(&now);
  printf("[RTC] %s ... UTC\r\n", rtc_ok?"DS3231 online":"soft clock");

  /* 7) 锁体执行器：上电先回到锁闭位 */
  servo_init();  servo_lock();  s_lock_engaged = true;

  /* 8) 键盘与唤醒键 + 卡表加载 */
  keypad_hardware_init();  wake_key_init();
  s_matrix_io.select_row = keypad_select_row;
  s_matrix_io.read_column = keypad_read_column;
  keypad_scan_init(&s_scan);  keypad_input_init(&s_keypad);
  (void)card_table_load();

  /* 9) 云端业务：注册 MQTT 下行回调并打印三元组 */
  app_wifi_init();
  watchdog_init();
  printf("[BOOT] smart lock board ready\r\n");
  return true;'''
code_box(s, Inches(0.7), Inches(1.5), SW-Inches(1.4), Inches(4.2), code, size=12)
callout(s, Inches(0.7), Inches(5.95), SW-Inches(1.4), Inches(1.05),
        "DS3231 在线判定一眼可见：启动日志出现 “[RTC] DS3231 online <UTC>” 即模块正常；"
        "若出现 “soft clock” 说明总线/I2C 地址没应答，断电时间会跳回编译时刻。", color=ACCENT)

# =====================================================================
# 8. Tasks & queues
# =====================================================================
s = new_content("并发", "FreeRTOS：5 任务 + 6 队列", 8)
rows = [
    ("任务", "优先级", "栈(字)", "周期/阻塞", "职责"),
    ("access_task", "4", "384", "100ms", "认证主线程：处理输入事件、菜单、锁控"),
    ("input_task", "3", "256", "10ms", "扫键盘/卡/指纹，产出 board_input_event"),
    ("network_task", "2", "768", "50ms", "ESP8266 MQTT 上行/下行、远程开锁"),
    ("log_task", "1", "256", "50ms", "消费 log_queue 写 W25Q64 环形日志"),
    ("ui_task", "1", "160", "阻塞", "刷 OLED；只被 ui_queue 唤醒"),
]
x0 = Inches(0.7); y0 = Inches(1.55)
widths = [Inches(2.5), Inches(1.0), Inches(1.1), Inches(1.6), Inches(5.4)]
rh = Inches(0.62)
for ri, row in enumerate(rows):
    x = x0
    for ci, cell in enumerate(cell := row):
        col = DARK if ri == 0 else (CARD if ri % 2 else RGBColor(0xEC,0xF1,0xF6))
        rect(s, x, y0+rh*ri, widths[ci], rh, color=col, line=RGBColor(0xCB,0xD5,0xE0))
        tb, tf = textbox(s, x+Pt(4), y0+rh*ri, widths[ci]-Pt(8), rh, anchor=MSO_ANCHOR.MIDDLE)
        add_para(tf, cell, 12 if ri else 11.5, HEAD_FG if ri==0 else DARK,
                 bold=(ri==0 or ci==0), font=FONT_UI, space_after=0)
        x = x + widths[ci]
callout(s, Inches(0.7), Inches(5.95), SW-Inches(1.4), Inches(1.0),
        "数据流向：input_task → input_queue → access_task → (grant/deny) → ui_queue/ui_task、"
        "log_queue/log_task；远程开锁来自 network_task → remote_queue → access_task。", color=ACCENT2)

# =====================================================================
# 9. Input subsystem
# =====================================================================
s = new_content("输入", "输入子系统：键盘 / 卡 / 指纹如何汇成一条事件", 9)
code = r'''/* 输入层把三类凭据统一成 board_input_event_t，喂给 access 任务 */
typedef enum {
  BOARD_AUTH_PIN, BOARD_AUTH_TOTP, BOARD_AUTH_VISITOR,
  BOARD_AUTH_FINGERPRINT, BOARD_AUTH_DURESS_FINGERPRINT,
  BOARD_AUTH_RFID, BOARD_AUTH_PERIODIC,
  BOARD_DOOR_OPENED, BOARD_DOOR_CLOSED,
  BOARD_TAMPER_TRIGGERED, BOARD_TOUCH_WAKE,
  BOARD_KEYPAD_ACTIVITY, BOARD_MENU_REQUEST, BOARD_MENU_KEY,
  BOARD_KEYPAD_CANCEL, BOARD_SHOW_RECENT_LOG, BOARD_AUTH_NONE
} board_input_type_t;

/* RC522 读卡（带重试 3 次 + 读后 HALT 回到 IDLE，避免重复触发） */
static bool read_card_uid(uint8_t *uid)
{
    for (int retry = 0; retry < 3; ++retry)
        if (MFRC522_ReadUid(uid)) { MFRC522_Halt(); return true; }
    return false;
}'''
code_box(s, Inches(0.7), Inches(1.5), SW-Inches(1.4), Inches(3.9), code, size=11.5)
callout(s, Inches(0.7), Inches(5.6), SW-Inches(1.4), Inches(1.35),
        "统一事件模型让认证逻辑只写一套 switch（见 handle_board_input）。"
        "读卡后必须 MFRC522_Halt() 让卡片回到 IDLE——否则同一张卡会被连续识别多次。"
        "指纹来自 USART2 的 AS608 模块；键盘是矩阵扫描（PA11/12/15、PB3 行，PB4~7 列）。", color=ACCENT2)

# =====================================================================
# 10. Auth flow
# =====================================================================
s = new_content("认证", "认证总体流程：一个事件如何变成“开锁”", 10)
steps = [
    ("input_task 产出 board_input_event（PIN/卡/指纹/TOTP…）", ACCENT),
    ("→ input_queue → access_task 收到，进入 handle_board_input()", ACCENT2),
    ("按 type 分支：先查锁定 → 胁迫码 → 业主码 → 第二因子 → 访客/周期码", DARK),
    ("命中：lock_controller_handle(AUTH_GRANTED) 置 UNLOCKED + 自动落锁 deadline", ACCENT2),
    ("grant_access()：set_physical_state() 解物理锁 + 反馈 + 审计", ACCENT),
    ("未命中：deny_access() 累加失败计数，满 3 次锁 60s", WARN),
]
y = Inches(1.55)
for i, (t, c) in enumerate(steps):
    rect(s, Inches(0.7), y, Inches(0.45), Inches(0.62), color=c)
    tb, tf = textbox(s, Inches(0.7), y, Inches(0.45), Inches(0.62), anchor=MSO_ANCHOR.MIDDLE)
    add_para(tf, str(i+1), 16, HEAD_FG, bold=True, align=PP_ALIGN.CENTER, space_after=0)
    rect(s, Inches(1.25), y, SW-Inches(1.95), Inches(0.62), color=CARD, line=RGBColor(0xCB,0xD5,0xE0))
    tb, tf = textbox(s, Inches(1.4), y, SW-Inches(2.2), Inches(0.62), anchor=MSO_ANCHOR.MIDDLE)
    add_para(tf, t, 13, DARK, font=FONT_UI, space_after=0)
    y = y + Inches(0.74)
callout(s, Inches(0.7), Inches(6.15), SW-Inches(1.4), Inches(0.8),
        "认证“决策”与“执行”分离：access 任务只改控制器状态，set_physical_state() 才真正动舵机/蜂鸣器/报警。", color=ACCENT)

# =====================================================================
# 11. lock_controller_handle (state machine)
# =====================================================================
s = new_content("锁控", "lock_controller_t：4 态状态机", 11)
code = r'''/* 状态：LOCKED / UNLOCKED / LOCKOUT / ALARM
   auto_lock_seconds 来自 smart_lock_app_start 的初始化参数 */
void lock_controller_init(lock_controller_t *c, uint32_t auto_lock_seconds)
{
    c->state = LOCK_STATE_LOCKED; c->door_closed = true;
    c->unlock_deadline = 0U; c->lockout_deadline = 0U;
    c->auto_lock_seconds = auto_lock_seconds;
}

void lock_controller_set_lockout(lock_controller_t *c, uint64_t until)
{
    c->state = LOCK_STATE_LOCKOUT;
    c->lockout_deadline = until; c->unlock_deadline = 0U;
}'''
code_box(s, Inches(0.7), Inches(1.5), SW-Inches(1.4), Inches(2.6), code, size=12)
bullets(s, Inches(0.7), Inches(4.3), SW-Inches(1.4), Inches(2.4), [
    ("LOCKED：默认态，舵机在锁闭位。", 0, DARK, False),
    ("UNLOCKED：授权通过，置 unlock_deadline = now + 自动落锁秒数。", 0, ACCENT2, False),
    ("LOCKOUT：连续失败达上限，time≥lockout_deadline 才回 LOCKED。", 0, WARN, False),
    ("ALARM：防拆触发，最高优先级；只有 CLEAR_ALARM 能退出。", 0, WARN, True),
], size=13.5, gap=8)

# =====================================================================
# 12. lock_controller_handle() impl
# =====================================================================
s = new_content("锁控", "lock_controller_handle() 完整实现", 12)
code = r'''lock_state_t lock_controller_handle(lock_controller_t *c,
                                lock_control_event_t event, uint64_t now)
{
    if (c == NULL) return LOCK_STATE_ALARM;

    if (event == LOCK_CONTROL_TAMPER) {            /* 防拆→报警，立即锁死 */
        c->state = LOCK_STATE_ALARM; c->unlock_deadline = 0U; return c->state;
    }
    if (event == LOCK_CONTROL_CLEAR_ALARM && c->state == LOCK_STATE_ALARM)
        { c->state = LOCK_STATE_LOCKED; return c->state; }
    if (c->state == LOCK_STATE_ALARM) return c->state;   /* 报警中无视其它 */
    if (c->state == LOCK_STATE_LOCKOUT) {               /* 锁定中 */
        if (event == LOCK_CONTROL_CLEAR_LOCKOUT ||
           (event == LOCK_CONTROL_TICK && now >= c->lockout_deadline))
            { c->state = LOCK_STATE_LOCKED; c->lockout_deadline = 0U; }
        return c->state;
    }
    switch (event) {
    case LOCK_CONTROL_AUTH_GRANTED:
    case LOCK_CONTROL_REMOTE_UNLOCK:
        c->state = LOCK_STATE_UNLOCKED;
        c->unlock_deadline = now + c->auto_lock_seconds; break;
    case LOCK_CONTROL_FORCE_LOCK:
        c->state = LOCK_STATE_LOCKED; c->unlock_deadline = 0U; break;
    case LOCK_CONTROL_DOOR_OPENED:  c->door_closed = false; break;
    case LOCK_CONTROL_DOOR_CLOSED:
        c->door_closed = true;
        if (c->state == LOCK_STATE_UNLOCKED)            /* 门已关→自动落锁 */
            { c->state = LOCK_STATE_LOCKED; c->unlock_deadline = 0U; }
        break;
    case LOCK_CONTROL_TICK:
        if (c->state == LOCK_STATE_UNLOCKED && c->unlock_deadline != 0U &&
            now >= c->unlock_deadline && c->door_closed)
            { c->state = LOCK_STATE_LOCKED; c->unlock_deadline = 0U; }
        break;
    default: break;
    }
    return c->state;
}'''
code_box(s, Inches(0.7), Inches(1.45), SW-Inches(1.4), Inches(5.2), code, size=10.3, line_spacing=1.0)

# =====================================================================
# 13. Credentials: HMAC + constant time
# =====================================================================
s = new_content("凭据", "凭据不存明文：HMAC 标签 + 常数时间比较", 13)
code = r'''/* app/src/lock_auth.c —— 防时序侧信道：比较耗时与内容无关 */
bool lock_constant_time_equal(const uint8_t *left, const uint8_t *right,
                              size_t length)
{
    uint8_t difference = 0U;
    if ((left == NULL) || (right == NULL)) return false;
    for (size_t i = 0U; i < length; ++i)
        difference |= (uint8_t)(left[i] ^ right[i]);   /* 永远比完，不提前 return */
    return difference == 0U;
}

/* 凭据存储为 HMAC(device_secret, PIN) 的摘要标签（非明文 PIN）
   校验：pin_credential_matches() 用 lock_constant_time_equal 比标签 */'''
code_box(s, Inches(0.7), Inches(1.5), SW-Inches(1.4), Inches(3.2), code, size=12)
callout(s, Inches(0.7), Inches(5.0), SW-Inches(1.4), Inches(1.6),
        "为什么不用 strcmp：strcmp 遇到首个不同字节就返回，比较时间随匹配前缀长度变化，"
        "攻击者可借响应耗时逐字符爆破 PIN。常数时间比较把差异“或”进一个字节，最后才判断，"
        "无论对多少位都相等，耗时恒定。device_secret 存于 security_config，掉电安全保存。", color=ACCENT)

# =====================================================================
# 14. pin_matches + lock_auth_verify_pin
# =====================================================================
s = new_content("凭据", "pin_matches()：虚拟前后缀 = 胁迫码机制", 14)
code = r'''/* 允许“虚拟前后缀”：在正确码前后输任意数字也算对 → 实现胁迫码 */
static bool pin_matches(const char *stored_pin, size_t stored_length,
                        const char *input, size_t input_length, bool allow_virtual)
{
    size_t last_start;
    if (stored_length == 0U || input_length < stored_length) return false;
    if ((!allow_virtual) && (input_length != stored_length)) return false;
    last_start = allow_virtual ? (input_length - stored_length) : 0U;
    uint8_t matched = 0U;
    for (size_t start = 0U; start <= last_start; ++start) {
        uint8_t difference = 0U;
        for (size_t i = 0U; i < stored_length; ++i)
            difference |= (uint8_t)stored_pin[i] ^ (uint8_t)input[start + i];
        if (difference == 0U) matched = 1U;          /* 子串命中即算对 */
    }
    return matched != 0U;
}

/* 上层：校验 + 失败计数 + 锁定；满 maximum_failures 锁 lockout_seconds */
lock_auth_result_t lock_auth_verify_pin(...) {
    if (now < state->locked_until) return LOCK_AUTH_LOCKED;
    if (pin_matches(...)) { state->failed_attempts = 0U; return LOCK_AUTH_GRANTED; }
    if (++state->failed_attempts >= policy->maximum_failures) {
        state->locked_until = now + policy->lockout_seconds;
        return LOCK_AUTH_LOCKED;
    }
    return LOCK_AUTH_DENIED;
}'''
code_box(s, Inches(0.7), Inches(1.45), SW-Inches(1.4), Inches(5.2), code, size=11.0, line_spacing=1.0)

# =====================================================================
# 15. TOTP principle
# =====================================================================
s = new_content("动态口令", "TOTP（RFC6238）：HOTP 的“时间版”", 15)
bullets(s, Inches(0.7), Inches(1.55), SW-Inches(1.4), Inches(3.0), [
    ("TOTP = HOTP(secret, counter)，counter = unix_time / time_step（默认 30s）。", 0, DARK, True),
    ("HOTP：把计数器按大端写入 8 字节 → HMAC-SHA1(secret, counter) → 动态截断取低 N 位。", 0, DARK, False),
    ("动态截断（RFC4226）：取摘要末字节低 4 位作 offset，取 4 字节并掩 0x7F 得 31 位整数。", 0, DARK, False),
    ("校验容窗 allowed_window=1：允许前后各一个时间步，抵消时钟/网络抖动。", 0, ACCENT2, False),
    ("出厂种子 totp_secret = \"12345678901234567890\"（RFC6238 附录 B 测试向量）。", 0, SUB, False),
], size=14, gap=10)
callout(s, Inches(0.7), Inches(5.0), SW-Inches(1.4), Inches(1.6),
        "注意：本工程 time_step=30、digits=6、window=1，与 Google Authenticator 完全兼容。"
        "校验时遍历 [now-step, now, now+step] 三个计数器，任一命中即放行。", color=ACCENT)

# =====================================================================
# 16. hotp_generate() impl
# =====================================================================
s = new_content("动态口令", "hotp_generate() 完整实现", 16)
code = r'''static uint32_t decimal_modulus(uint8_t digits)   /* 10^digits */
{ uint32_t m = 1U; for (uint8_t i=0U;i<digits;++i) m*=10U; return m; }

static uint32_t hotp_generate(const uint8_t *secret, size_t secret_length,
                              uint64_t counter, uint8_t digits)
{
    uint8_t counter_bytes[8]; uint8_t digest[SHA1_DIGEST_SIZE];
    /* 计数器大端（网络序）写入 */
    for (size_t i = 0U; i < 8; ++i)
        counter_bytes[7U - i] = (uint8_t)(counter >> (i * 8U));
    /* HMAC-SHA1 */
    hmac_sha1(secret, secret_length, counter_bytes, 8, digest);
    /* 动态截断 */
    uint8_t offset = (uint8_t)(digest[19] & 0x0F);
    uint32_t binary = ((uint32_t)(digest[offset]   & 0x7F) << 24U) |
                      ((uint32_t)(digest[offset+1] & 0xFF) << 16U) |
                      ((uint32_t)(digest[offset+2] & 0xFF) <<  8U) |
                      ((uint32_t)(digest[offset+3] & 0xFF));
    return binary % decimal_modulus(digits);     /* 取低 digits 位 */
}'''
code_box(s, Inches(0.7), Inches(1.5), SW-Inches(1.4), Inches(4.6), code, size=12)
callout(s, Inches(0.7), Inches(6.25), SW-Inches(1.4), Inches(0.7),
        "digest[19] 是 SHA1 第 20 字节（索引 19）；offset∈[0,15]，取 digest[offset..offset+3]。"
        "0x7F 掩掉符号位，保证结果为正 31 位整数。", color=ACCENT)

# =====================================================================
# 17. totp_verify() impl
# =====================================================================
s = new_content("动态口令", "totp_verify() 完整实现", 17)
code = r'''bool totp_verify(const uint8_t *secret, size_t secret_length,
                 uint64_t unix_time, uint32_t time_step, uint8_t digits,
                 uint32_t candidate, uint8_t allowed_window)
{
    const uint64_t base = (time_step == 0U) ? 0U : unix_time / time_step;
    if (secret==NULL || secret_length==0U || time_step==0U ||
        digits<6U || digits>8U || allowed_window>2U) return false;

    for (int32_t delta = -(int32_t)allowed_window;
         delta <= (int32_t)allowed_window; ++delta) {
        uint64_t counter;
        if ((delta < 0) && (base < (uint64_t)(-delta))) continue; /* 防下溢 */
        counter = (delta < 0) ? base - (uint64_t)(-delta)
                              : base + (uint64_t)delta;
        if (hotp_generate(secret, secret_length, counter, digits) == candidate)
            return true;                                   /* 任一步命中即放行 */
    }
    return false;
}

/* 调用方（handle_board_input 的 BOARD_AUTH_TOTP 分支）：
   totp_verify(totp_secret, totp_secret_length, now, 30U, 6U, code, 1U) */'''
code_box(s, Inches(0.7), Inches(1.45), SW-Inches(1.4), Inches(5.1), code, size=10.5, line_spacing=1.0)

# =====================================================================
# 18. handle_board_input() — overview
# =====================================================================
s = new_content("调度", "handle_board_input()：认证总入口（结构）", 18)
code = r'''static void handle_board_input(const board_input_event_t *event, uint64_t now)
{
    last_activity_at = now;            /* 防“静止 60s 熄屏”把输入截断 */
    switch (event->type) {
    case BOARD_AUTH_PIN:            /* 密码：含胁迫/业主/第二因子/访客/失败 */
    case BOARD_AUTH_TOTP:           /* 动态口令：6 位数字 → totp_verify */
    case BOARD_AUTH_VISITOR:        /* APP 一次性临时码 */
    case BOARD_AUTH_FINGERPRINT:    /* 指纹：单因子 or 置 pending 第一因子 */
    case BOARD_AUTH_DURESS_FINGERPRINT:  /* 胁迫指纹 → grant_duress */
    case BOARD_AUTH_RFID:           /* 卡：单因子 or 置 pending 第一因子 */
    case BOARD_AUTH_PERIODIC:       /* 周期口令 */
    case BOARD_DOOR_OPENED:         /* 门磁开 → 锁控 DOOR_OPENED */
    case BOARD_DOOR_CLOSED:         /* 门磁关 → 自动落锁 + 审计 */
    case BOARD_TAMPER_TRIGGERED:    /* 防拆 → 报警 */
    case BOARD_MENU_REQUEST:        /* 'D'：进管理菜单（先验主密码/30s 免验） */
    case BOARD_MENU_KEY:            /* 菜单态原始按键，交 menu_key() */
    case BOARD_KEYPAD_CANCEL:       /* 'C'：退出菜单 */
    default: break;
    }
}'''
code_box(s, Inches(0.7), Inches(1.5), SW-Inches(1.4), Inches(4.7), code, size=11.5)
callout(s, Inches(0.7), Inches(6.35), SW-Inches(1.4), Inches(0.6),
        "所有凭据走同一个 switch；“决策”在函数内完成，“执行”交给 grant/deny。下两页拆 PIN 与 TOTP/卡 分支。", color=ACCENT)

# =====================================================================
# 19. handle_board_input() PIN branch
# =====================================================================
s = new_content("调度", "handle_board_input()：PIN 分支（含胁迫/第二因子/访客）", 19)
code = r'''case BOARD_AUTH_PIN:
    if (keypad_is_locked(now)) { deny_access(PIN, id, true, now); }
    else if (pin_credential_matches(&duress_pin, secret, secret_len,
                                    digits, n, true))
        grant_duress(PIN, id, now);                 /* 胁迫码：静默开+告警 */
    else if (pin_credential_matches(&owner_pin, secret, secret_len,
                                    digits, n, true)) {
        keypad_clear_failures();
        if (second_factor_mode == DISABLED)
            grant_access(PIN, id, now);
        else if ((pending_first_factor_until >= now) && 匹配第一因子)
            { clear_pending_factor(); grant_access(PIN, user_id, now); }
        else { clear_pending_factor(); deny_access(PIN,id,false,now);
               queue_ui("Use first factor first"); }
    }
    else if (visitor_credential_verify_and_consume(&visitor, secret,
              secret_len, digits, n, now)) {        /* ★ 临时密码修复点 */
        keypad_clear_failures();
        if (board_security_save(&security_config))
            grant_access(VISITOR, id, now);         /* 一次性消耗后落盘 */
    }
    else deny_access(PIN, id, keypad_register_failure(now), now);
    break;'''
code_box(s, Inches(0.7), Inches(1.5), SW-Inches(1.4), Inches(4.7), code, size=11.3)
callout(s, Inches(0.7), Inches(6.35), SW-Inches(1.4), Inches(0.6),
        "顺序即优先级：胁迫 > 业主 > 第二因子 > 访客 > 失败。访客码刻意绕过第二因子（访客无指纹/卡）。", color=ACCENT)

# =====================================================================
# 20. handle_board_input() TOTP/RFID/fingerprint/door
# =====================================================================
s = new_content("调度", "handle_board_input()：TOTP / 卡 / 指纹 / 门磁", 20)
code = r'''case BOARD_AUTH_TOTP:                       /* 动态口令 */
    if (keypad_is_locked(now)) { deny_access(TOTP, id, true, now); break; }
    for (i=0;i<n;++i) if 非数字 { deny; return; }
        else submitted = submitted*10 + (digits[i]-'0');
    if (n==6U && totp_verify(totp_secret, len, now, 30U, 6U, submitted, 1U))
        { keypad_clear_failures(); grant_access(TOTP, id, now); }
    else deny_access(TOTP, id, keypad_register_failure(now), now);
    break;

case BOARD_AUTH_RFID:                        /* 卡：双因子时先记第一因子 */
    if (second_factor_mode == RFID_PIN) {
        pending_first_factor = RFID; pending_first_factor_user = id;
        pending_first_factor_until = now + SECOND_FACTOR_TIMEOUT;
        queue_ui("Enter PIN");
    } else { grant_access(RFID, id, now);
        if (board_card_last_role()==ADMIN) admin_grace_until = now+30; }

case BOARD_DOOR_CLOSED:                     /* 门关 → 自动落锁 */
    lock_controller_handle(&controller, DOOR_CLOSED, now);
    door_opened_at = 0U; set_physical_state(controller.state);
    queue_audit(LOCK_EVENT_LOCK, SYSTEM, 0U, 1U, now);
    break;

case BOARD_TAMPER_TRIGGERED:                /* 防拆 → 报警 */
    lock_controller_handle(&controller, TAMPER, now);
    set_physical_state(controller.state);
    queue_ui("Tamper alarm"); queue_audit(TAMPER, SYSTEM, 0U, 0U, now);
    break;'''
code_box(s, Inches(0.7), Inches(1.45), SW-Inches(1.4), Inches(5.2), code, size=10.6, line_spacing=1.0)

# =====================================================================
# 21. grant / deny / set_physical_state
# =====================================================================
s = new_content("执行", "grant_access / deny_access / set_physical_state", 21)
code = r'''/* 授权：改状态→解物理锁→反馈→界面→审计（落盘在调用方已完成） */
static void set_physical_state(lock_state_t state)
{
    board_lock_set(state != LOCK_STATE_UNLOCKED);   /* 非 UNLOCKED 都锁上 */
    board_alarm_set(state == LOCK_STATE_ALARM);     /* 仅 ALARM 拉报警 */
}
static void grant_access(lock_auth_method_t method, uint16_t user_id, uint64_t now)
{
    (void)lock_controller_handle(&controller, LOCK_CONTROL_AUTH_GRANTED, now);
    set_physical_state(controller.state);
    board_auth_feedback(true);
    queue_ui(controller.state, "Access granted", true);
    queue_audit(LOCK_EVENT_UNLOCK, method, user_id, 1U, now);
}
static void deny_access(lock_auth_method_t method, uint16_t user_id,
                        bool locked_out, uint64_t now)
{
    if (locked_out) {
        lock_controller_set_lockout(&controller, pin_auth_state.locked_until);
        set_physical_state(controller.state);
        queue_ui(controller.state, "Try again later", true);
        queue_audit(LOCK_EVENT_LOCKOUT, method, user_id, 0U, now);
    } else {
        board_auth_feedback(false);
        queue_ui(controller.state, "Access denied", true);
        queue_audit(LOCK_EVENT_AUTH_FAILURE, method, user_id, 0U, now);
    }
}'''
code_box(s, Inches(0.7), Inches(1.5), SW-Inches(1.4), Inches(4.6), code, size=11.3)
callout(s, Inches(0.7), Inches(6.25), SW-Inches(1.4), Inches(0.65),
        "决策与执行解耦的红利：锁控逻辑只改 controller.state，set_physical_state() 才是唯一真正动硬件的点，"
        "便于做“双因子未齐/超时”时只改状态不解锁。", color=ACCENT2)

# =====================================================================
# 22. config_store envelope
# =====================================================================
s = new_content("存储", "config_store：掉电安全的 A/B 双槽信封", 22)
code = r'''/* 信封格式（小端），覆盖 magic..payload 整体算 CRC32：
   offset 0  : magic   'S''L''C''1' = 0x31434C53
   offset 4  : version u16
   offset 6  : payload_length u16
   offset 8  : sequence u32
   offset 12 : payload[payload_length]
   末尾 4 字节: crc32（覆盖 magic..payload）

   #define CONFIG_STORE_MAGIC   0x31434C53
   #define CONFIG_STORE_SLOT_SIZE 4096U          // W25Q64 一个扇区

   读：两槽都解析，取“CRC 正确且 sequence 较大”的那个。
   写：永远写“当前非活动”槽（先擦后写），sequence = 活动序号 + 1。
       擦/写中途断电 → 旧槽仍完好，配置不丢。 */
bool config_store_load(const config_store_media_t *m, board_security_config_t *c);
bool config_store_save(const config_store_media_t *m,
                       const board_security_config_t *c);  // 写非活动槽'''
code_box(s, Inches(0.7), Inches(1.5), SW-Inches(1.4), Inches(4.6), code, size=11.5)
callout(s, Inches(0.7), Inches(6.25), SW-Inches(1.4), Inches(0.65),
        "payload 逐字段编码（不 memcpy 结构体），避开编译器填充字节的跨平台差异。"
        "A/B 槽 + 序号 + CRC 三重保护，让管理员密码/卡片表在意外断电时不损坏。", color=ACCENT2)

# =====================================================================
# 23. event_log CRC32 ring
# =====================================================================
s = new_content("审计", "event_log：CRC32 校验 + 环形日志", 23)
code = r'''/* CRC32（IEEE 802.3 多项式 0xEDB88320），与以太网一致 */
uint32_t event_log_crc32(const void *data, size_t length)
{
    const uint8_t *b = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0U; i < length; ++i) {
        crc ^= b[i];
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            uint32_t mask = (uint32_t)(-(int32_t)(crc & 1U));
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}
bool event_log_record_valid(const event_log_record_t *r)
{   /* magic 不符 或 CRC 不符 → 无效（挡住写一半的坏记录） */
    if (r==NULL || r->magic != EVENT_LOG_MAGIC) return false;
    return event_log_crc32(r, offsetof(event_log_record_t, crc32)) == r->crc32;
}

/* 环形写：擦当前槽→写→槽指针+1 取模；sequence 用 int32 环绕比较更新 */
bool event_log_append(event_log_t *log, event_log_record_t *rec) {
    rec->magic = EVENT_LOG_MAGIC; rec->sequence = log->next_sequence;
    rec->crc32 = event_log_crc32(rec, offsetof(event_log_record_t, crc32));
    if (!log->storage.erase(log->next_slot)) return false;
    if (!log->storage.write(log->next_slot, rec)) return false;
    log->next_slot = (log->next_slot + 1U) % slot_count;
    ++log->next_sequence; return true;
}
/* sequence_is_newer(a,b): (int32_t)(a-b) > 0  —— 正确处理序号回绕 */'''
code_box(s, Inches(0.7), Inches(1.45), SW-Inches(1.4), Inches(5.2), code, size=10.4, line_spacing=1.0)

# =====================================================================
# 24. RTC DS3231
# =====================================================================
s = new_content("时钟", "DS3231 高精度时钟：断电靠 VBAT 保持", 24)
bullets(s, Inches(0.7), Inches(1.55), SW-Inches(1.4), Inches(3.6), [
    ("芯片内存 UTC；OLED/菜单/审计统一用北京时间（DS3231_LOCAL_TZ_HOURS=8）。", 0, DARK, False),
    ("DS3231_USE_SOFT=0：优先真实模块；无 ACK 自动回落软时钟并打 “[RTC] soft clock…”。", 0, DARK, False),
    ("断电准确：DS3231 由纽扣电池 VBAT 供电，模块掉电时间不丢；软时钟断电跳回编译时刻。", 0, ACCENT2, True),
    ("动态口令依赖时钟：偏差 >30s（一个 time_step）会导致 TOTP 校验失败 → 必须保证时钟准。", 0, WARN, False),
    ("共享软 I2C（PB8/PB9）与 OLED，故需总线互斥锁（见下页）。", 0, SUB, False),
], size=13.5, gap=9)
callout(s, Inches(0.7), Inches(5.4), SW-Inches(1.4), Inches(1.2),
        "启动日志确认在线：[RTC] DS3231 online 2026-09-15 10:00:00 UTC。"
        "若只见 soft clock，先查 I2C 地址 0xD0 与 VBAT 是否装电池。", color=ACCENT)

# =====================================================================
# 25. I2C mutex — root cause & fix
# =====================================================================
s = new_content("显示", "★ OLED 文字错乱根因：I2C 总线并发", 25)
bullets(s, Inches(0.7), Inches(1.55), SW-Inches(1.4), Inches(3.4), [
    ("现象：屏幕随机花屏、文字错乱（指纹/密码正常，因为不碰屏）。", 0, WARN, True),
    ("根因：OLED(0x78) 与 DS3231(0xD0) 共用同一条软 I2C(PB8/PB9)。", 0, DARK, False),
    ("access_task(4) 读 DS3231、ui_task(1) 刷整屏 OLED、input_task(3) 键盘回显——", 0, DARK, False),
    ("高优先级在“一次 I2C 事务传到一半”时抢断低优先级，START/数据/STOP 交织 → 从机收错半字节。", 0, DARK, False),
    ("老修法只锁 OLED（s_oled_mtx）挡不住 DS3231，所以没根治。", 0, SUB, False),
    ("正确修法：把锁下沉到“事务层”——任一 START..STOP 独占总线，且用递归锁（oled.c 会嵌套进入）。", 0, ACCENT2, True),
], size=13, gap=7)
callout(s, Inches(0.7), Inches(5.3), SW-Inches(1.4), Inches(1.3),
        "关键结论：同一条 I2C 总线上挂多个器件时，互斥必须加在“总线事务”层，"
        "而不是某个器件的驱动层——否则任一器件的访问都能撕开别人的事务。", color=ACCENT)

# =====================================================================
# 26. i2c_lock / s_i2c_mtx impl
# =====================================================================
s = new_content("显示", "i2c_lock() / s_i2c_mtx：事务级递归锁实现", 26)
code = r'''static SemaphoreHandle_t s_i2c_mtx = NULL;     /* 总线互斥锁（递归） */

static void i2c_lock(void)
{
    /* 调度器未启动时是单线程，无需加锁（锁也可能还没建） */
    if (s_i2c_mtx && xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        xSemaphoreTakeRecursive(s_i2c_mtx, portMAX_DELAY);
}
static void i2c_unlock(void)
{
    if (s_i2c_mtx && xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        xSemaphoreGiveRecursive(s_i2c_mtx);
}

void IIC_Init(void) {                         /* 一定先于 OLED/DS3231 Init */
    ... GPIO_Pin_8|9 = Out_PP 高; IIC_SCL_H(); IIC_SDA_H();
    if (s_i2c_mtx == NULL)
        s_i2c_mtx = xSemaphoreCreateRecursiveMutex();  /* 全局唯一，建一次 */
}

/* 每个对外接口都把“整段事务”包在锁里 */
uint8_t IIC_WriteBytesRaw(uint8_t dev, const uint8_t *d, uint16_t len){
    i2c_lock();  iic_start(); ... 整段收发 ...; i2c_unlock(); return ok; }
uint8_t IIC_ReadRegs(uint8_t dev, uint8_t reg, uint8_t *buf, uint16_t len){
    i2c_lock();  iic_start(); ... Repeated START 读 ...; i2c_unlock(); return ok; }
uint8_t IIC_WriteReg(uint8_t dev, uint8_t reg, uint8_t val){
    i2c_lock();  iic_start(); ...; i2c_unlock(); return ok; }'''
code_box(s, Inches(0.7), Inches(1.45), SW-Inches(1.4), Inches(5.2), code, size=10.6, line_spacing=1.0)

# =====================================================================
# 27. RC522 heal — root cause
# =====================================================================
s = new_content("驱动", "★ RC522“用一阵就失灵”根因", 27)
bullets(s, Inches(0.7), Inches(1.55), SW-Inches(1.4), Inches(3.6), [
    ("现象：ID 卡刚开始能识别开锁，时间一长彻底没反应（指纹/密码正常）。", 0, WARN, True),
    ("老判据错在哪：用 VersionReg(0x37) 读回固定硅片 ID（本板恒 0x92）判断“芯片活着”。", 0, DARK, False),
    ("致命问题：芯片哪怕掉电复位、把 Init 写进去的配置全丢了，VersionReg 照样读出 0x92；", 0, DARK, False),
    ("       射频/命令状态机卡死时 VersionReg 也正常 → 老逻辑只做 AntennaOn，配置永远补不回。", 0, DARK, False),
    ("真实诱因：ESP8266 峰值 300mA+ 与 RC522 共用 3V3，导致 RC522 棕色复位丢配置。", 0, SUB, False),
    ("正确判据：回读 Init 写过的 TModeReg(0x8D)/TPrescalerReg(0x3E) 才代表配置真的在。", 0, ACCENT2, True),
], size=12.5, gap=7)
callout(s, Inches(0.7), Inches(5.4), SW-Inches(1.4), Inches(1.2),
        "结论：永远不要用“只读固定 ID 寄存器”当健康探针。要回读你亲手写下去的配置寄存器，"
        "丢了就整片重初始化。", color=ACCENT)

# =====================================================================
# 28. card_reader_heal() impl
# =====================================================================
s = new_content("驱动", "card_reader_heal()：两级自愈实现", 28)
code = r'''/* 连续多轮无卡时的两级自愈（2026-09-15 加固）
   CARD_HEAL_AFTER_POLLS=50（约 5s 无卡触发一次）
   CARD_REINIT_AFTER_HEAL=6（每 6 次自愈无条件重初始化，≈30s） */
static void card_reader_heal(void)
{
    const bool regs_ok = (MFRC522_Check() != 0U);   /* 回读 TMode/TPrescaler */

    if ((!regs_ok) || (++s_card_heal_count >= CARD_REINIT_AFTER_HEAL)) {
        s_card_heal_count = 0U;
        printf("[RC522] heal: %s, re-init\r\n",
               regs_ok ? "periodic refresh" : "config lost");
        MFRC522_Init();                              /* 整片重初始化补回配置 */
        if (!MFRC522_Check())
            printf("[RC522] WARN re-init failed -- check SCK/MOSI/MISO/PWR\r\n");
    } else {
        MFRC522_AntennaOn();                         /* 天线位被清掉就补开 */
    }
}

/* 调用点：poll_card() 连续无卡累计达阈值才触发，场上无卡才做（耗时 ~13ms 安全） */
if (!read_card_uid(uid)) {
    if (++s_card_heal_streak >= CARD_HEAL_AFTER_POLLS) {
        s_card_heal_streak = 0U; card_reader_heal();   /* 自愈 */
    }
    return false;
}
s_card_heal_streak = 0U;   /* 读到卡即清零，避免误触发 */'''
code_box(s, Inches(0.7), Inches(1.45), SW-Inches(1.4), Inches(5.2), code, size=10.8, line_spacing=1.0)

# =====================================================================
# 29. AS608 fingerprint
# =====================================================================
s = new_content("驱动", "AS608 指纹模块：USART2 异步协议", 29)
bullets(s, Inches(0.7), Inches(1.55), SW-Inches(1.4), Inches(3.8), [
    ("通信：USART2（PA2/PA3），模块上电即等待指令包，校验和收尾。", 0, DARK, False),
    ("用户页：AS608_USER_PAGE_MAX=48（1~47 普通用户，48=管理员指纹）。", 0, DARK, False),
    ("胁迫指纹：AS608_DURESS_PAGE=49；匹配到即 BOARD_AUTH_DURESS_FINGERPRINT。", 0, WARN, True),
    ("录入：app 层指令 PS_RegModel / PS_Store；删除 PS_DeleteChar。", 0, DARK, False),
    ("指纹存模块内部 Flash，擦 W25Q64 不会清指纹——与密码/卡解耦。", 0, ACCENT2, False),
    ("48 页管理员指纹开锁后给 30s 免验窗口（admin_grace_until），可直接按 'D' 进菜单。", 0, SUB, False),
], size=13, gap=8)
callout(s, Inches(0.7), Inches(5.7), SW-Inches(1.4), Inches(0.95),
        "双因子模式（FINGERPRINT_PIN）下，指纹只置 pending_first_factor，必须再输 PIN 才开锁——"
        "指纹被胁迫时攻击者拿不到门。", color=ACCENT)

# =====================================================================
# 30. ESP8266 network
# =====================================================================
s = new_content("网络", "ESP-12F：USART1 + MQTT 远程开锁", 30)
bullets(s, Inches(0.7), Inches(1.55), SW-Inches(1.4), Inches(3.8), [
    ("模组：ESP-12F（安信可 4MB/32Mbit、26MHz），刷 32Mbit 版 AT 固件。", 0, DARK, False),
    ("通信：USART1（PA9/AF_PP, PA10/浮空）@115200；app_wifi.c 复用，不随便改。", 0, DARK, False),
    ("业务：network_task 维护 MQTT 长连，下行“远程开锁”→ remote_queue → access_task。", 0, DARK, False),
    ("峰值电流 300mA+：与 RC522 共用 3V3 时要就近并 100µF+0.1µF，否则棕色复位（读卡掉）。", 0, WARN, True),
    ("诊断开关 SMART_LOCK_ESP8266_LINK_PROBE：平时 0；崩溃死循环时开它抓日志。", 0, SUB, False),
], size=13, gap=8)
callout(s, Inches(0.7), Inches(5.7), SW-Inches(1.4), Inches(0.95),
        "实测结论：MCU 侧 USART1 寄存器、GPIO、时钟全部正常——连不上网是 ESP8266 固件崩溃死循环，"
        "重刷稳定 AT 固件或换模块才能根治（刷新 STM32 救不了）。", color=ACCENT)

# =====================================================================
# 31. Security summary
# =====================================================================
s = new_content("安全", "安全设计要点总览", 31)
items = [
    ("凭据不存明文：以 HMAC(device_secret, PIN) 标签存储，校验走常数时间比较。", 0, DARK, True),
    ("胁迫机制：虚拟前后缀让“正确码+随意数字”算对；胁迫码/指纹静默开 + 后台告警。", 0, DARK, False),
    ("防爆破：连续 3 次失败锁 60s；菜单入口复用同一失败计数，防止连按 'D' 绕过。", 0, DARK, False),
    ("双因子：FINGERPRINT_PIN / RFID_PIN 模式先记第一因子，再输 PIN 才放行。", 0, DARK, False),
    ("掉电安全：config A/B 双槽 + 序号 + CRC32；event_log 每条 CRC 校验挡坏记录。", 0, DARK, False),
    ("决策/执行分离：lock_controller 只改状态，set_physical_state 才动舵机/报警。", 0, DARK, False),
    ("防侧信道：常数时间比较 + TOTP 时间窗，避免耗时/时钟泄露。", 0, DARK, False),
]
bullets(s, Inches(0.7), Inches(1.55), SW-Inches(1.4), Inches(4.8), items, size=13.5, gap=10)
callout(s, Inches(0.7), Inches(6.5), SW-Inches(1.4), Inches(0.45),
        "一句话：把“密码学正确”和“工程健壮”分开做，再在接口处用状态机与队列串起来。", color=ACCENT)

# =====================================================================
# 32. Unlock sequence diagram
# =====================================================================
s = new_content("时序", "一次“密码开锁”的端到端时序", 32)
code = r'''键盘输入 PIN
   │  (keypad_input → BOARD_AUTH_PIN)
   ▼
input_task ──input_queue──▶ access_task::handle_board_input
   │   pin_credential_matches 比对 HMAC 标签（常数时间）
   │   命中 → lock_controller_handle(AUTH_GRANTED)
   │           state = UNLOCKED, unlock_deadline = now + 自动落锁秒
   ▼
grant_access()
   ├─ set_physical_state()        → board_lock_set(false) 舵机解锁
   ├─ board_auth_feedback(true)   → 蜂鸣/绿灯
   ├─ queue_ui("Access granted") ──ui_queue──▶ ui_task 刷 OLED
   └─ queue_audit(UNLOCK)         ──log_queue──▶ log_task 写 W25Q64
   ▼
门开 → BOARD_DOOR_OPENED → 锁控 door_closed=false
门闭 → BOARD_DOOR_CLOSED → 锁控自动落锁 + set_physical_state(LOCKED)
   ▼
超时（TICK, now≥deadline 且门关）→ 自动 LOCKED（兜底自动落锁）'''
code_box(s, Inches(0.7), Inches(1.5), SW-Inches(1.4), Inches(4.6), code, size=11.5)
callout(s, Inches(0.7), Inches(6.25), SW-Inches(1.4), Inches(0.65),
        "注意三路并行：物理解锁、界面反馈、审计落盘都通过队列异步完成，access 任务不阻塞。", color=ACCENT2)

# =====================================================================
# 33. Build / flash / debug
# =====================================================================
s = new_content("工程", "构建 / 烧录 / 调试（实测命令）", 33)
code = r'''# 无头编译（UV4 是 GUI 子系统，须 GUI 子 shell；一律 -r 强制 Rebuild）
Start-Process -FilePath UV4.exe -ArgumentList '-r','SmartLock.uvprojx','-j0','-o',log `
            -Wait -PassThru ; 读 ExitCode(0/1/2) 与 log 中 "Program Size:" 判真重编

# 烧录（SWD，-rst 烧完即跑；ESP8266 不随 MCU 复位）
STM32_Programmer_CLI.exe -c port=SWD -w build/SmartLock.hex -v -rst

# 调试串口 COM7@115200 8N1（printf 走 USART3 PB10/PB11）
# 实测体积：Code=55776 RO=5896 RW=400 ZI=17672，0err/0warn；ROM 62072/65536、RAM 18072/20480
# 热插拔实时读寄存器（不复位不打断）：
STM32_Programmer_CLI.exe -c port=SWD mode=hotplug -r32 <addr> <len>

# 中文源码必须 UTF-8 带 BOM（否则 AC5 报 #8: missing closing quote）'''
code_box(s, Inches(0.7), Inches(1.5), SW-Inches(1.4), Inches(4.4), code, size=10.8)
callout(s, Inches(0.7), Inches(6.05), SW-Inches(1.4), Inches(0.85),
        "ROM 余量极小（仅 ~3.5KB）：省空间先砍字库不砍安全逻辑。栈溢出会触发 vApplicationStackOverflowHook"
        " 进入“静默死机”——输出停在某行中间、xTickCount 冻结。", color=ACCENT)

# =====================================================================
# 34. Stack / silent death
# =====================================================================
s = new_content("排错", "“静默死机”：栈溢出定位法", 34)
code = r'''/* 任务栈（字数）：input 256 / access 384 / network 768 / log 256 / ui 160
   网络栈曾仅 320 字（42%）实测溢出：esp8266_mqtt_connect 局部 392B */
vApplicationStackOverflowHook() { taskDISABLE_INTERRUPTS(); for(;;); }  /* 不打印！ */

/* 定位：map 查 *_task_stack 地址 → 热插拔读栈底 16 字节
   FreeRTOS 用 0xA5A5A5A5 填充未用栈；被改写=溢出（内容常是返回地址→map 反查函数） */
STM32_Programmer_CLI.exe -c port=SWD mode=hotplug -r32 <stack_base_addr> 16

/* 特征：输出停在中间 + xTickCount 冻结 + CFSR=0&HFSR=0
   + SHCSR 有 pending + ICSR.VECTACTIVE=14(PendSV) + DHCSR.S_HALT=0 */
/* 优先级配置：configUSE_TIMERS=0（关定时器省 RAM）；configTOTAL_HEAP_SIZE=4KB 全静态 */'''
code_box(s, Inches(0.7), Inches(1.5), SW-Inches(1.4), Inches(4.4), code, size=11.3)
callout(s, Inches(0.7), Inches(6.05), SW-Inches(1.4), Inches(0.85),
        "栈溢出不报错只死机，最难查。黄金法则：network 栈给够（现 768 字），RAM 吃紧先关 configUSE_TIMERS，"
        "别去压别的任务栈。", color=WARN)

# =====================================================================
# 35. Menu
# =====================================================================
s = new_content("交互", "管理菜单（待机按 'D' 进入）", 35)
bullets(s, Inches(0.7), Inches(1.55), SW-Inches(1.4), Inches(4.0), [
    ("入口：先验主密码；管理员指纹(48页)/管理卡开锁后 30s 内按 'D' 免验直达。", 0, DARK, False),
    ("按键：菜单页 A/C 移动、D 进入、B 返回；输入页 数字/#确认、B 删位、C 退出。", 0, DARK, False),
    ("菜单树：密码管理(主/胁迫) · 指纹(录1~47/删1~49/管理48) · 卡片(角色 flags 位0) ·", 0, DARK, False),
    ("        胁迫(49页) · 系统(改时间 YYMMDDHHMMSS / 记录 / 组合)。", 0, DARK, False),
    ("状态机在 access 任务（menu_poll≈100ms），渲染经 ui 队列 → board_menu_show()。", 0, DARK, False),
    ("中文文案在 board_port.c（字库扫描约定）；OLED 每行补满 128px 防残像。", 0, SUB, False),
], size=13, gap=8)
callout(s, Inches(0.7), Inches(5.8), SW-Inches(1.4), Inches(0.95),
        "菜单态不喂键盘扫描、不 queue_ui、不进 STOP 低功耗——保证按键实时响应。改主密码“先落盘再报成功”。", color=ACCENT)

# =====================================================================
# 36. Runtime conventions
# =====================================================================
s = new_content("约定", "运行时约定速查", 36)
items = [
    ("时钟：DS3231 存 UTC，显示+8；无 ACK 回落软时钟（断电跳回编译时刻）。", 0, DARK, False),
    ("FreeRTOS V10.5.1（RVDS 移植），全部静态分配，configTOTAL_HEAP_SIZE=4KB。", 0, DARK, False),
    ("I2C：OLED(0x78) + DS3231(0xD0) 共用 PB8/PB9，事务级递归锁 s_i2c_mtx。", 0, DARK, False),
    ("W25Q64：配置 A/B 槽 0x000000/0x001000，卡表 0x002000，日志尾部 128 扇区。", 0, DARK, False),
    ("凭据：出厂业主 PIN 123456 / 胁迫 654321；TOTP 种子 12345678901234567890。", 0, DARK, False),
    ("事件持久化：event_type 按 uint8_t 写盘，新增枚举只能追加末尾（不重用旧值）。", 0, DARK, False),
]
bullets(s, Inches(0.7), Inches(1.55), SW-Inches(1.4), Inches(4.4), items, size=13.5, gap=11)
callout(s, Inches(0.7), Inches(6.2), SW-Inches(1.4), Inches(0.6),
        "指纹在 AS608 模块里，擦 W25Q64 不清指纹；卡片角色 flags 位0=管理员。", color=ACCENT2)

# =====================================================================
# 37. Key files map
# =====================================================================
s = new_content("导航", "关键文件 → 职责 速查表", 37)
rows = [
    ("文件", "职责"),
    ("firmware/stm32/src/main.c", "入口：smart_lock_app_start → vTaskStartScheduler"),
    ("firmware/stm32/src/smart_lock_app.c", "任务/队列、handle_board_input、grant/deny、菜单"),
    ("firmware/stm32/src/board_port.c", "board_init、读卡、键盘、菜单渲染、总线"),
    ("firmware/app/src/lock_controller.c", "4 态锁控状态机"),
    ("firmware/app/src/lock_auth.c", "常数时间比较、PIN 虚拟前后缀校验"),
    ("firmware/app/src/totp.c", "HOTP/TOTP 动态截断 + 校验"),
    ("firmware/app/src/config_store.c", "A/B 双槽 CRC32 信封"),
    ("firmware/app/src/event_log.c", "CRC32 环形审计日志"),
    ("firmware/HARDWARE/i2c_soft.c", "软 I2C + 事务级互斥锁"),
    ("firmware/HARDWARE/mfrc522.c", "RC522 读卡 + Check/Init 自愈"),
]
x0 = Inches(0.7); y0 = Inches(1.5); w1=Inches(5.4); w2=SW-Inches(1.4)-w1; rh=Inches(0.46)
for ri, (a,b) in enumerate(rows):
    col = DARK if ri==0 else (CARD if ri%2 else RGBColor(0xEC,0xF1,0xF6))
    rect(s, x0, y0+rh*ri, w1, rh, color=col, line=RGBColor(0xCB,0xD5,0xE0))
    rect(s, x0+w1, y0+rh*ri, w2, rh, color=col, line=RGBColor(0xCB,0xD5,0xE0))
    tb, tf = textbox(s, x0+Pt(4), y0+rh*ri, w1-Pt(8), rh, anchor=MSO_ANCHOR.MIDDLE)
    add_para(tf, a, 11 if ri else 11.5, HEAD_FG if ri==0 else RGBColor(0x1F,0x6F,0xB2),
             bold=(ri==0 or ri==0), font="Consolas" if ri else FONT_UI, space_after=0)
    tb, tf = textbox(s, x0+w1+Pt(4), y0+rh*ri, w2-Pt(8), rh, anchor=MSO_ANCHOR.MIDDLE)
    add_para(tf, b, 11, HEAD_FG if ri==0 else DARK, font=FONT_UI, space_after=0)

# =====================================================================
# 38. Common bugs recap
# =====================================================================
s = new_content("踩坑", "三大“偶发 / 玄学”Bug 复盘", 38)
items = [
    ("① 屏幕文字错乱 → I2C 总线并发：锁加在事务层（s_i2c_mtx 递归锁），不是器件层。", 0, WARN, True),
    ("② 读卡一阵后失灵 → RC522 棕色复位丢配置：用 MFRC522_Check() 回读配置，两级自愈重 Init。", 0, WARN, True),
    ("③ 临时密码开不了锁 → handle_board_input 的 PIN 分支从未校验 visitor 凭据：补 visitor 分支。", 0, WARN, True),
    ("④ 静默死机 → 网络栈溢出：栈给到 768 字，关 configUSE_TIMERS 省 RAM。", 0, DARK, False),
    ("⑤ 断电时间漂移 → 必须用真实 DS3231 + VBAT；软时钟仅兜底。", 0, DARK, False),
    ("⑥ 模块崩溃死循环 → ESP8266 固件问题：重刷稳定 AT 或换模块，非 MCU 责任。", 0, DARK, False),
]
bullets(s, Inches(0.7), Inches(1.55), SW-Inches(1.4), Inches(4.8), items, size=13.5, gap=11)
callout(s, Inches(0.7), Inches(6.5), SW-Inches(1.4), Inches(0.45),
        "共性原则：先分“MCU 责任 / 模块责任”，再用寄存器回读 + 日志定位，别凭感觉改代码。", color=ACCENT)

# =====================================================================
# 39. 40-min speaking plan
# =====================================================================
s = new_content("讲解", "40–50 分钟讲解节奏分配", 39)
plan = [
    ("模块①② 启动与并发（P4–P8）", "约 8 min：main→app_start→board_init 9步→任务队列"),
    ("模块③ 认证核心（P9–P14）", "约 10 min：输入模型→状态机→常数时间→虚拟前后缀"),
    ("模块④ 动态口令（P15–P17）", "约 6 min：RFC6238 原理→hotp_generate→totp_verify"),
    ("模块⑤ 持久化与安全（P22–P23,31）", "约 7 min：A/B 信封→CRC32 日志→安全总览"),
    ("模块⑥ 驱动与排错（P24–P30,33–34）", "约 12 min：DS3231/I2C锁/RC522自愈/AS608/ESP8266/栈溢出"),
    ("收尾（P32,35–P38）", "约 5 min：时序图→菜单→文件导航→踩坑复盘→答疑"),
]
y = Inches(1.55)
for t, d in plan:
    rect(s, Inches(0.7), y, SW-Inches(1.4), Inches(0.8), color=CARD, line=RGBColor(0xCB,0xD5,0xE0))
    tb, tf = textbox(s, Inches(0.9), y, SW-Inches(1.8), Inches(0.8), anchor=MSO_ANCHOR.MIDDLE)
    add_para(tf, t, 14, ACCENT, bold=True, font=FONT_UI, space_after=1)
    add_para(tf, d, 12, DARK, font=FONT_UI, space_after=0)
    y = y + Inches(0.92)
callout(s, Inches(0.7), Inches(6.45), SW-Inches(1.4), Inches(0.5),
        "每页源码先念函数签名，再逐段讲“为什么这么写”，最后点设计取舍——照此节奏正好 40+ 分钟。", color=ACCENT2)

# =====================================================================
# 40. Summary
# =====================================================================
s = new_content("总结", "本课要点回顾", 40)
items = [
    ("启动极薄：main 只起应用+调度器；所有初始化在 app_start/board_init。", 0, DARK, False),
    ("并发靠队列：5 任务 + 6 队列，输入→决策→执行全异步，access 不阻塞。", 0, DARK, False),
    ("认证可证安全：HMAC 标签 + 常数时间比较 + 虚拟前后缀胁迫 + 双因子 + 失败锁定。", 0, DARK, False),
    ("持久化掉电安全：A/B 双槽信封 + CRC32 日志，写一半断电不丢配置。", 0, DARK, False),
    ("两个“玄学 Bug”已根治：I2C 事务锁（不乱码）、RC522 配置回读自愈（不失灵）。", 0, WARN, True),
    ("排错方法论：先定责任边界，再用寄存器热读 + 日志定位，栈溢出看 0xA5 填充。", 0, DARK, False),
]
bullets(s, Inches(0.7), Inches(1.55), SW-Inches(1.4), Inches(4.6), items, size=14, gap=12)
callout(s, Inches(0.7), Inches(6.3), SW-Inches(1.4), Inches(0.55),
        "源码已逐函数贴出，配套 docs/code_walkthrough.html 含交互流程图，可对照讲解。", color=ACCENT2)

# =====================================================================
# 41. Q&A / Thanks
# =====================================================================
s = add_slide()
bg(s, DARK)
rect(s, 0, Inches(2.9), SW, Inches(1.7), color=ACCENT)
tb, tf = textbox(s, Inches(0.9), Inches(2.95), SW-Inches(1.8), Inches(1.6), anchor=MSO_ANCHOR.MIDDLE)
add_para(tf, "Q & A · 谢谢观看", 30, HEAD_FG, bold=True, space_after=6, font=FONT_UI)
add_para(tf, "代码讲解到此结束，欢迎就任一函数实现或踩坑细节提问。", 15,
         RGBColor(0xCF,0xE6,0xFF), space_after=0, font=FONT_UI)
tb2, tf2 = textbox(s, Inches(0.9), Inches(5.0), SW-Inches(1.8), Inches(1.2))
add_para(tf2, "配套资料：docs/code_walkthrough.html（交互版，含 10 张流程图）", 12,
         RGBColor(0x9F,0xB3,0xC8), space_after=4, font=FONT_UI)
add_para(tf2, "固件工程：SmartLock.uvprojx · 烧录：STM32_Programmer_CLI -c port=SWD -w SmartLock.hex -v -rst",
         11, RGBColor(0x9F,0xB3,0xC8), space_after=0, font=FONT_UI)

# =====================================================================
# 42. (blank extra for duration buffer) — testing checklist
# =====================================================================
s = new_content("实测", "上板自查清单（讲解时可现场演示）", 42)
items = [
    ("[ ] 启动日志出现 [OK] MFRC522 RFID (Version 0x92) 与 [RTC] DS3231 online。", 0, DARK, False),
    ("[ ] [CARD] N card(s) loaded 与 [BOOT] smart lock board ready。", 0, DARK, False),
    ("[ ] 密码 123456 开锁、输错 3 次后 60s 内被锁（Try again later）。", 0, DARK, False),
    ("[ ] 胁迫码 654321 开锁但后台记录为胁迫事件。", 0, WARN, False),
    ("[ ] 临时密码 / TOTP 一次性开锁成功。", 0, DARK, False),
    ("[ ] 长按卡贴住 5s 以上无花屏（I2C 锁生效）；连续读卡 10min 仍响应（RC522 自愈）。", 0, ACCENT2, True),
    ("[ ] 门关闭后自动落锁；超时未关门也自动落锁。", 0, DARK, False),
]
bullets(s, Inches(0.7), Inches(1.55), SW-Inches(1.4), Inches(4.8), items, size=13.5, gap=11)
callout(s, Inches(0.7), Inches(6.5), SW-Inches(1.4), Inches(0.45),
        "把这张清单当“验收用例”，讲解完当场跑一遍最有说服力。", color=ACCENT)

# ---------------- save ----------------
out = os.path.abspath(PPTX_PATH)
os.makedirs(os.path.dirname(out), exist_ok=True)
prs.save(out)
print("SAVED", out, "slides=", len(prs.slides._sldIdLst))
