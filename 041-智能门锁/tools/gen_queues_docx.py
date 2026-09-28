# -*- coding: utf-8 -*-
"""
生成《队列生产者与消费者梳理》Word 文档：
- 内嵌队列数据流图 PNG
- 六条队列总表
- 每条队列：生产者函数 / 消费者函数详解 + 坑
全部使用中文弯引号 “ ”，避免 ASCII " 截断 Python 字符串。
"""
from docx import Document
from docx.shared import Pt, RGBColor, Inches
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml.ns import qn
from docx.oxml import OxmlElement

CJK = "微软雅黑"
MONO = "Consolas"
DARK = RGBColor(0x1B, 0x26, 0x31)
ACCENT = RGBColor(0x1F, 0x5C, 0x8B)
RED = RGBColor(0xC0, 0x39, 0x2B)
GREEN = RGBColor(0x1E, 0x8A, 0x4C)
BLUE = RGBColor(0x1F, 0x61, 0x8D)
CODE_BG = "F2F4F7"
NOTE_BG = "FBF3E2"

doc = Document()
style = doc.styles["Normal"]
style.font.name = CJK
style.font.size = Pt(10.5)
style.element.rPr.rFonts.set(qn("w:eastAsia"), CJK)

def _cjk(run):
    run.font.name = CJK
    run._element.rPr.rFonts.set(qn("w:eastAsia"), CJK)

def h1(text):
    p = doc.add_heading(level=1)
    r = p.add_run(text); r.font.name = CJK; r.font.color.rgb = ACCENT; r.font.size = Pt(16); _cjk(r)

def h2(text):
    p = doc.add_heading(level=2)
    r = p.add_run(text); r.font.name = CJK; r.font.color.rgb = DARK; r.font.size = Pt(13); _cjk(r)

def para(text, bold=False, color=DARK, size=10.5):
    p = doc.add_paragraph()
    r = p.add_run(text); r.font.name = CJK; r.font.size = Pt(size); r.font.bold = bold; r.font.color.rgb = color; _cjk(r)
    return p

def bullet(text, color=DARK):
    p = doc.add_paragraph(style="List Bullet")
    r = p.add_run(text); r.font.name = CJK; r.font.size = Pt(10); r.font.color.rgb = color; _cjk(r)

def code(text):
    p = doc.add_paragraph()
    p.paragraph_format.left_indent = Inches(0.18)
    p.paragraph_format.space_before = Pt(3); p.paragraph_format.space_after = Pt(6)
    shd = OxmlElement("w:shd"); shd.set(qn("w:val"), "clear")
    shd.set(qn("w:color"), "auto"); shd.set(qn("w:fill"), CODE_BG)
    p.paragraph_format.element.pPr.append(shd)
    lines = text.split("\n")
    for i, line in enumerate(lines):
        r = p.add_run(line); r.font.name = MONO; r.font.size = Pt(8.3); r.font.color.rgb = DARK
        r._element.rPr.rFonts.set(qn("w:eastAsia"), MONO)
        if i != len(lines) - 1:
            r.add_break()

def callout(title, text):
    p = doc.add_paragraph()
    p.paragraph_format.left_indent = Inches(0.12)
    p.paragraph_format.space_before = Pt(4); p.paragraph_format.space_after = Pt(8)
    shd = OxmlElement("w:shd"); shd.set(qn("w:val"), "clear")
    shd.set(qn("w:color"), "auto"); shd.set(qn("w:fill"), NOTE_BG)
    p.paragraph_format.element.pPr.append(shd)
    r = p.add_run("【" + title + "】"); r.font.name = CJK; r.font.size = Pt(10); r.font.bold = True; r.font.color.rgb = RED; _cjk(r)
    r2 = p.add_run(text); r2.font.name = CJK; r2.font.size = Pt(10); r2.font.color.rgb = DARK; _cjk(r2)

def table(rows, widths):
    t = doc.add_table(rows=len(rows), cols=len(rows[0]))
    t.style = "Table Grid"
    for ri, row in enumerate(rows):
        for ci, cell in enumerate(row):
            c = t.rows[ri].cells[ci]
            c.width = Inches(widths[ci])
            p = c.paragraphs[0]
            r = p.add_run(cell)
            r.font.name = CJK; r.font.size = Pt(9); _cjk(r)
            r.font.bold = (ri == 0)
            r.font.color.rgb = DARK if ri else RGBColor(0xFF, 0xFF, 0xFF)
            if ri == 0:
                shd = OxmlElement("w:shd"); shd.set(qn("w:val"), "clear")
                shd.set(qn("w:color"), "auto"); shd.set(qn("w:fill"), "1F5C8B")
                c._tc.get_or_add_tcPr().append(shd)
    return t

# ---------------- 封面 ----------------
t = doc.add_paragraph(); t.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = t.add_run("智能门锁 · 六条队列 生产者 / 消费者 梳理")
r.font.name = CJK; r.font.size = Pt(24); r.font.bold = True; r.font.color.rgb = ACCENT; _cjk(r)
sub = doc.add_paragraph(); sub.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = sub.add_run("基于 smart_lock_app.c 真实代码逐处核对    |    2026-09-18")
r.font.name = CJK; r.font.size = Pt(11); r.font.color.rgb = DARK; _cjk(r)

# ---------------- 说明 ----------------
h1("先纠正一点：是 6 条，不是 5 条")
para("代码里在 smart_lock_app_start() 中共创建了 6 条队列（smart_lock_app.c:2179-2202，全部 xQueueCreateStatic 静态分配）。"
     "你说的“五个任务”对应 5 条主数据流队列，第 6 条 log_request_queue 是“菜单查日志”的专用请求队列，容易漏数。")

# ---------------- 数据流图 ----------------
h1("一、数据流总图")
doc.add_picture(r"C:\Users\71563\Desktop\block\docs\queue_dataflow.png", width=Inches(6.6))
doc.paragraphs[-1].alignment = WD_ALIGN_PARAGRAPH.CENTER

# ---------------- 总表 ----------------
h1("二、六条队列总表")
table([
    ["队列", "元素类型", "容量", "生产者（任务 · 函数）", "消费者（任务 · 函数）"],
    ["input_queue", "board_input_event_t", "12",
     "input_task → board_poll_input() 产事件\nxQueueSend 超时 20ms",
     "access_task\nxQueueReceive(100ms)\n→ handle_board_input()"],
    ["remote_queue", "remote_command_t", "4",
     "network_task → board_network_poll_command()\nxQueueSend 超时 20ms",
     "access_task\nwhile xQueueReceive(0) 清空队列\n→ handle_remote_command()"],
    ["log_queue", "event_log_record_t", "8",
     "access_task 内 queue_audit()\n（grant/deny/duress 后调用）xQueueSend(0)",
     "log_task\nxQueueReceive(50ms)\n→ event_log_append()"],
    ["notify_queue", "board_notification_t", "8",
     "access_task 内 queue_audit()\n与 log_queue 同帧双投 xQueueSend(0)",
     "network_task\nxQueueReceive(0)\n→ board_network_publish()"],
    ["ui_queue", "app_ui_message_t", "6",
     "access_task 内 queue_ui() /\nqueue_menu() / queue_ui_log()\nxQueueSend(0)",
     "ui_task\nxQueueReceive(portMAX_DELAY)\n→ board_ui_show / board_menu_show"],
    ["log_request_queue", "uint32_t（第 N 条偏移）", "4",
     "菜单逻辑 menu_log_key() / menu_key()\n翻看记录时 xQueueSend(0)",
     "log_task\nwhile xQueueReceive(0)\n→ event_log_read_recent()"],
], [1.15, 1.5, 0.5, 2.35, 2.1])

callout("一眼看懂结构",
        "access_task 是枢纽：它是 input_queue / remote_queue 的【消费者】，同时是 log_queue / notify_queue / ui_queue 的【生产者】"
        "（经 queue_audit / queue_ui / queue_menu / queue_ui_log 四个辅助函数投递）。其余任务各管一摊：input 只产、"
        "network 下产上消、log 只消、ui 只消。数据只往一个方向流，绝不回环。")

# ---------------- 每条队列详解 ----------------
h1("三、逐条队列：生产者 / 消费者函数详解")

h2("3.1  input_queue（input_task → access_task）")
para("生产者函数（smart_lock_app.c:1876-1881，input_task 内）：", bold=True)
code('''if (board_poll_input(&event))                     /* 板级层采集，产出统一事件 */
{
    if (xQueueSend(input_queue, &event, pdMS_TO_TICKS(20U)) != pdTRUE)
        board_diagnostic_report(BOARD_DIAG_INPUT_QUEUE_FULL);  /* 满则丢弃+诊断 */
}
vTaskDelay(pdMS_TO_TICKS(10U));                   /* 100Hz 节拍 */''')
para("消费者函数（smart_lock_app.c:1895-1898，access_task 内）：", bold=True)
code('''if (xQueueReceive(input_queue, &event, pdMS_TO_TICKS(100U)) == pdTRUE)
    handle_board_input(&event, board_unix_time());  /* 交给认证决策分派 */''')
bullet("事件由 board_poll_input() 统一打包：键盘拼满的 PIN、刷卡 UID、指纹命中、PA8 唤醒、菜单按键，全部封装成 board_input_event_t。")
bullet("生产超时 20ms：队列满时宁可丢一次输入也不阻塞采集节拍（蜂鸣/RGB 状态机不能停）。")
bullet("消费阻塞 100ms：既保证按键即时响应，又给同任务里的 menu_poll() 留出固定节奏。")
callout("坑", "队列满打 [DIAG] input queue full —— 通常意味着 access_task 被什么卡住了（如 Flash 写阻塞），是排查“按键没反应”的第一线索。")

h2("3.2  remote_queue（network_task → access_task）")
para("生产者函数（smart_lock_app.c:2052-2058，network_task 内）：", bold=True)
code('''if (board_network_poll_command(&command))        /* 内部先 app_wifi_process() 驱动 AT 状态机 */
{
    if (xQueueSend(remote_queue, &command, pdMS_TO_TICKS(20U)) != pdTRUE)
        board_diagnostic_report(BOARD_DIAG_REMOTE_QUEUE_FULL);
    memset(&command, 0, sizeof(command));         /* 投完即清，防残留 */
}''')
para("消费者函数（smart_lock_app.c:1903-1909，access_task 内）：", bold=True)
code('''while (xQueueReceive(remote_queue, &command, 0U) == pdTRUE)  /* 0 超时：一次清空 */
{
    last_activity_at = now;
    handle_remote_command(&command, now);          /* 验签+防重放+执行 */
    memset(&command, 0, sizeof(command));
}''')
bullet("云端下行链路：ESP8266 AT 收数据 → app_wifi 解析 → s_remote_ring 环形缓冲 → network_task 搬进 remote_queue → access_task 消费。")
bullet("消费用 0 超时 while：把积压的命令一口气处理完，避免云端连发时排队延迟。")
callout("坑", "远程命令必须先验签（remote_command_verify 的 HMAC + request_id 防重放）才执行；验不过只记审计，不给攻击者任何回执。")

h2("3.3  log_queue（access_task[queue_audit] → log_task）")
para("生产者函数 queue_audit（smart_lock_app.c:402-434）：", bold=True)
code('''static void queue_audit(lock_event_type_t event_type, lock_auth_method_t method,
                        uint16_t user_id, uint8_t result, uint64_t now)
{
    event_log_record_t record = { .unix_time=now, .user_id=user_id,
        .event_type=(uint8_t)event_type, .auth_method=(uint8_t)method, .result=result, ... };
    board_notification_t notification = { .state=controller.state, .event_type=event_type, ... };
    if (xQueueSend(log_queue, &record, 0U) != pdTRUE)
        board_diagnostic_report(BOARD_DIAG_LOG_QUEUE_FULL);      /* Flash 写不进，丢 */
    if (xQueueSend(notify_queue, &notification, 0U) != pdTRUE)
        board_diagnostic_report(BOARD_DIAG_NOTIFY_QUEUE_FULL);   /* 云上报不出去，丢 */
}''')
para("消费者函数（smart_lock_app.c:2087-2093，log_task 内）：", bold=True)
code('''if (xQueueReceive(log_queue, &record, pdMS_TO_TICKS(50U)) == pdTRUE)
{
    if (!event_log_append(&event_log, &record))   /* 填信封+擦+写 W25Q64 */
        board_diagnostic_report(BOARD_DIAG_LOG_STORAGE_FAILURE);
}''')
bullet("一次审计事件 → 同时投 log_queue（落盘）和 notify_queue（上云），两条队列各走各的消费者，互不阻塞。")
bullet("record 的 magic/sequence/crc32 由消费者侧 event_log_append 填 —— 谁落盘谁填信封，生产者不用关心持久化细节。")
callout("坑", "Flash 擦写（毫秒级）必须收敛在 log_task 一个任务里做，否则会和其它任务的 W25Q64 访问撞总线。生产者只投队列，绝不自己写 Flash。")

h2("3.4  notify_queue（access_task[queue_audit] → network_task）")
para("生产者：同一个 queue_audit() 双投（见 3.3 代码第二个 xQueueSend）。", bold=True)
para("消费者函数（smart_lock_app.c:2061-2075，network_task 内）：", bold=True)
code('''if (xQueueReceive(notify_queue, &notification, 0U) == pdTRUE)
{
    uint32_t retry_delay_ms = 250U;
    while (!board_network_publish(&notification))     /* 失败指数退避重试 */
    {
        board_diagnostic_report(BOARD_DIAG_NETWORK_PUBLISH_FAILURE);
        vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
        if (retry_delay_ms < 8000U) retry_delay_ms *= 2U;   /* 250→500→...→8000ms 封顶 */
    }
}''')
bullet("board_network_publish 内部按事件类型分派：unlock/lock → push_door_event + push_status；duress/tamper/door_ajar/lockout → push_alarm。")
bullet("离线时 app_wifi 的 push 会静默丢弃，所以 publish 基本不会失败 —— 重试循环主要是防 MQTT 瞬断。")
callout("坑", "物模型 method 只认 0~5，本工程 lock_auth_method_t 有 0~7，board_network_publish 里的 thing_model_method() 必须做映射（访客/周期码归 1），否则平台回 2409 参数错误。")

h2("3.5  ui_queue（access_task[queue_*] → ui_task）")
para("三个生产者函数（smart_lock_app.c:240 / 273 / 301）：", bold=True)
code('''queue_ui(state, message, power_on)   /* 待机/状态帧：菜单激活时直接 return 不投 */
queue_menu(page, sel, aux, line)     /* 菜单帧：is_menu=true，ui_task 改走 board_menu_show */
queue_ui_log(record, ok)             /* 日志帧：菜单内外两种渲染路径 */''')
para("消费者函数（smart_lock_app.c:2117-2140，ui_task 内）：", bold=True)
code('''if (xQueueReceive(ui_queue, &ui, portMAX_DELAY) == pdTRUE)  /* 永久阻塞：有消息才醒 */
{
    board_ui_power(ui.power_on);                           /* 先点/关屏 */
    if (ui.is_menu) { ... board_menu_show(...) 或 board_menu_show_log(...); }
    else if (ui.power_on && ui.has_log_record) { board_ui_show_log(&ui.log_record); }
    else if (ui.power_on) { board_ui_show(ui.state, ...); }
}''')
bullet("ui_task 用 portMAX_DELAY 永久阻塞 —— 平时零 CPU 占用，屏幕静止时系统才能安心进 STOP。")
bullet("queue_ui 里有 menu_active 守卫：菜单显示期间待机/状态消息一律不投，防止把菜单画面顶掉。")
callout("坑", "所有 UI 更新必须经 ui_queue 单消费者串行化 + 软 I2C 事务级递归锁，否则 OLED(0x78) 与 DS3231(0xD0) 共用 PB8/PB9 会被不同优先级任务交织访问 → 花屏（本项目的经典坑）。")

h2("3.6  log_request_queue（菜单逻辑 → log_task）")
para("生产者函数（smart_lock_app.c:1157 menu_log_key / 1183 menu_key）：", bold=True)
code('''/* 用户在菜单“开锁记录”页按 A/C 翻页时 */
uint32_t log_offset = 当前浏览位置;
if (xQueueSend(log_request_queue, &log_offset, 0U) != pdTRUE) { /* 丢 */ }''')
para("消费者函数（smart_lock_app.c:2096-2106，log_task 内）：", bold=True)
code('''while (xQueueReceive(log_request_queue, &offset, 0U) == pdTRUE)  /* 0 超时清空 */
{
    if (event_log_read_recent(&event_log, offset, &record))
        queue_ui_log(&record, true);      /* 查到 → 投 ui_queue 显示 */
    else
        queue_ui_log(NULL, false);        /* 没这条 → UI 显示“无记录” */
}''')
bullet("这是一条“请求-应答”队列：菜单发偏移量 → log_task 读 Flash → queue_ui_log 把结果经 ui_queue 送回屏幕。")
bullet("菜单逻辑自己绝不直接读 W25Q64 —— 保持“Flash 只有 log_task 摸”的铁律。")
callout("坑", "菜单翻页可能连按多键产生多个请求，消费者用 while(0 超时) 一次清空，避免旧请求堆积导致翻页“迟滞感”。")

# ---------------- 并发要点 ----------------
h1("四、贯穿六条队列的并发要点")
bullet("全部 xQueueCreateStatic：队列存储在编译期静态数组（*_queue_storage），绝不运行时 malloc —— 4KB 堆根本不够动态分配。")
bullet("值拷贝语义：xQueueSend 把结构体按字节拷进队列，生产者投完改本地副本不影响队列里的数据，天然免锁。")
bullet("发送超时两档：决策链路上的发送（input 20ms / remote 20ms）给短超时防饿死；审计/UI 类发送一律 0 超时，满了就丢 + 诊断，绝不反压决策任务。")
bullet("接收超时三档：100ms（access 主循环节奏）/ 50ms（log 轮询）/ portMAX_DELAY（ui 事件驱动）/ 0（清空式消费）。")
bullet("队列满是诊断线索：六种 BOARD_DIAG_*_QUEUE_FULL 分别对应六条队列，串口看到哪条满，就知道哪个消费者被卡住了。")
bullet("数据单向流：input/remote → access → log/notify/ui，无回环、无任务间共享可写状态（除 I2C 总线由事务锁保护），这是全项目并发的安全底线。")

out = r"C:\Users\71563\Desktop\block\docs\队列生产者消费者梳理.docx"
doc.save(out)
print("SAVED =", out)
