# -*- coding: utf-8 -*-
"""
生成《智能门锁 5 个任务函数详解》Word 文档。
逐任务拆解：任务函数本体（逐行/逐段）+ 它调用的每个子函数（签名/位置/逐段解释/坑）。
全部使用中文弯引号 “ ” ，避免 ASCII " 截断 Python 字符串。
"""
from docx import Document
from docx.shared import Pt, RGBColor, Inches
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml.ns import qn
from docx.oxml import OxmlElement

# ---------- 样式常量 ----------
CJK = "微软雅黑"
MONO = "Consolas"
DARK = RGBColor(0x1B, 0x26, 0x31)
ACCENT = RGBColor(0x1F, 0x5C, 0x8B)
RED = RGBColor(0xC0, 0x39, 0x2B)
GREEN = RGBColor(0x1E, 0x8A, 0x4C)
CODE_BG = "F2F4F7"
NOTE_BG = "FBF3E2"

doc = Document()

# 默认正文字体（中文）
style = doc.styles["Normal"]
style.font.name = CJK
style.font.size = Pt(10.5)
style.element.rPr.rFonts.set(qn("w:eastAsia"), CJK)

def _set_cjk(run):
    run.font.name = CJK
    r = run._element
    r.rPr.rFonts.set(qn("w:eastAsia"), CJK)

def h1(text):
    p = doc.add_heading(level=1)
    run = p.add_run(text)
    run.font.name = CJK
    run.font.color.rgb = ACCENT
    run.font.size = Pt(17)
    _set_cjk(run)
    return p

def h2(text):
    p = doc.add_heading(level=2)
    run = p.add_run(text)
    run.font.name = CJK
    run.font.color.rgb = DARK
    run.font.size = Pt(13.5)
    _set_cjk(run)
    return p

def h3(text):
    p = doc.add_heading(level=3)
    run = p.add_run(text)
    run.font.name = CJK
    run.font.color.rgb = DARK
    run.font.size = Pt(11.5)
    _set_cjk(run)
    return p

def para(text, bold=False, color=DARK, size=10.5):
    p = doc.add_paragraph()
    run = p.add_run(text)
    run.font.name = CJK
    run.font.size = Pt(size)
    run.font.bold = bold
    run.font.color.rgb = color
    _set_cjk(run)
    return p

def code(text):
    """等宽灰底代码块，保留换行。"""
    p = doc.add_paragraph()
    p.paragraph_format.left_indent = Inches(0.18)
    p.paragraph_format.space_before = Pt(3)
    p.paragraph_format.space_after = Pt(6)
    # 灰底
    shd = OxmlElement("w:shd")
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:color"), "auto")
    shd.set(qn("w:fill"), CODE_BG)
    p.paragraph_format.element.pPr.append(shd)
    for i, line in enumerate(text.split("\n")):
        run = p.add_run(line)
        run.font.name = MONO
        run.font.size = Pt(8.3)
        run.font.color.rgb = DARK
        r = run._element
        r.rPr.rFonts.set(qn("w:eastAsia"), MONO)
        if i != len(text.split("\n")) - 1:
            run.add_break()
    return p

def bullets(items, color=DARK):
    for it in items:
        lvl, txt = (it if isinstance(it, tuple) else (0, it))
        p = doc.add_paragraph(style="List Bullet" if lvl == 0 else "List Bullet 2")
        run = p.add_run(txt)
        run.font.name = CJK
        run.font.size = Pt(10)
        run.font.color.rgb = color
        _set_cjk(run)
    return p

def callout(title, text):
    """黄色提示框（坑/要点）。"""
    p = doc.add_paragraph()
    p.paragraph_format.left_indent = Inches(0.12)
    p.paragraph_format.space_before = Pt(4)
    p.paragraph_format.space_after = Pt(8)
    shd = OxmlElement("w:shd")
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:color"), "auto")
    shd.set(qn("w:fill"), NOTE_BG)
    p.paragraph_format.element.pPr.append(shd)
    r = p.add_run("【" + title + "】")
    r.font.name = CJK
    r.font.size = Pt(10)
    r.font.bold = True
    r.font.color.rgb = RED
    _set_cjk(r)
    r2 = p.add_run(text)
    r2.font.name = CJK
    r2.font.size = Pt(10)
    r2.font.color.rgb = DARK
    _set_cjk(r2)
    return p

# ===================================================================
# 封面
# ===================================================================
title = doc.add_paragraph()
title.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = title.add_run("智能门锁 · 五个任务函数逐行详解")
r.font.name = CJK; r.font.size = Pt(26); r.font.bold = True; r.font.color.rgb = ACCENT
_set_cjk(r)
sub = doc.add_paragraph(); sub.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = sub.add_run("input / access / network / log / ui    —— 含每个任务所调用的全部子函数")
r.font.name = CJK; r.font.size = Pt(12); r.font.color.rgb = DARK
_set_cjk(r)
meta = doc.add_paragraph(); meta.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = meta.add_run("基于 STM32F103 + FreeRTOS V10.5.1 工程    |    源码级讲解    |    2026-09-16")
r.font.name = CJK; r.font.size = Pt(10); r.font.color.rgb = RGBColor(0x70,0x70,0x70)
_set_cjk(r)

# ===================================================================
# 0. 概览
# ===================================================================
h1("第 0 章  总览：五任务与并发骨架")
para("整个应用层被切成 5 个 FreeRTOS 任务，它们之间不直接互相调用，全部通过“队列”传递数据。"
     "access_task 是唯一的“决策中心”，其余任务只负责采集（input）、联网（network）、落盘（log）、"
     "显示（ui）。下面这张表是全局坐标。")

code(
'''优先级  任务        栈(字)   周期/阻塞方式           角色
 4      access     384      xQueueReceive(input,100ms)  决策中心：认证/状态机/STOP/看门狗
 3      input      256      vTaskDelay(10ms)            采集键盘/卡/指纹/唤醒 → input_queue
 2      network    768      vQueueReceive(notify,0)     下行命令→remote_queue；上行→云平台
 1      log        256      xQueueReceive(log,50ms)     审计记录环形写 Flash；应答查日志
 1      ui         160      xQueueReceive(ui,portMAX)   收到 UI 消息才刷新 OLED''')

para("六条队列（全部 xQueueCreateStatic，编译期分配，绝不在运行时 malloc）：")
bullets([
    "input_queue：input_task → access_task，元素 board_input_event_t（统一输入事件）",
    "remote_queue：network_task → access_task，元素 remote_command_t（云端下发的开锁/锁门/发访客码）",
    "log_queue：各任务 → log_task，元素 event_log_record_t（一条审计记录）",
    "notify_queue：各任务 → network_task，元素 board_notification_t（要上报云端的事件）",
    "ui_queue：各任务 → ui_task，元素 app_ui_message_t（整屏/菜单/日志帧）",
    "log_request_queue：菜单逻辑 → log_task，元素 uint32_t（查第 N 条最新记录）",
])
callout("为什么用队列而不是函数直调",
        "抢占式 RTOS 里“直接调”= 让一个任务替另一个任务干活，会同时带来 5 个问题：① 破坏单一职责，"
        "② 高优先级任务被低优先级函数的长耗时拖死，③ 跨任务共享状态必须加锁，④ 无法解耦便于 PC 端单元测试，"
        "⑤ 栈/时序不可控。队列把“数据”而非“控制流”在任务间搬运，每层只在自己的节拍里消费。")

# ===================================================================
# 第 1 章 input_task
# ===================================================================
h1("第 1 章  input_task（优先级 3，栈 256 字）")
para("最低层的采集器。10 ms 节拍轮询一切输入源，把原始事件打包成 board_input_event_t，投进 input_queue。"
     "它自己不做任何判断，判断全部交给 access_task。")

h2("1.1  input_task 函数本体（smart_lock_app.c:1843）")
code(
'''static void input_task(void *argument)
{
    board_input_event_t event;
    (void)argument;
    for (;;)
    {
        /* 10 ms 节拍：与 buzzer_process()/rgb_process() 的设计周期一致，
         * 也让键盘消抖（KEYPAD_DEBOUNCE_MS=25）有足够的采样拍数。 */
        if (board_poll_input(&event))
        {
            if (xQueueSend(input_queue, &event, pdMS_TO_TICKS(20U)) != pdTRUE)
            {
                board_diagnostic_report(BOARD_DIAG_INPUT_QUEUE_FULL);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10U));
    }
}''')
bullets([
    "for(;;)：任务体是个死循环，FreeRTOS 任务永远不能 return，否则触发 configASSERT。",
    "board_input_event_t event：在栈上分配一次，每次轮询复用，避免反复申请。",
    "(void)argument：静态任务没有参数，显式丢弃以消编译器警告。",
    "board_poll_input(&event)：真正的采集逻辑（见 1.2），返回 true 表示“这次采到一个事件”。",
    "xQueueSend(input_queue, &event, 20ms)：把事件值拷贝进队列。20ms 超时是防队列满时永久阻塞——"
    "input 优先级(3)高于 access(4)? 不，注意优先级数值越大越高，access=4 最高，input=3，"
    "access 会尽快取走，所以 20ms 几乎不会等满；若真满则丢弃并上报诊断。",
    "vTaskDelay(10ms)：主动让出 CPU 10 ms，驱动 100Hz 节拍，也保证键盘消抖能采到连续几拍。",
])
callout("坑：用 vTaskDelay 而非空转",
        "若写成 while(1) 不 delay，input 会 100% 占满 CPU，access(优先级4)虽能抢占，但系统整体调度变糟、"
        "功耗也高。10ms 这个值是和蜂鸣/RGB 状态机、键盘消抖共同作用定出来的。")

h2("1.2  input_task 调用的子函数")
h3("board_poll_input()  —— 采集总入口（board_port.c:662）")
code(
'''bool board_poll_input(board_input_event_t *event)
{
    if (event == NULL) return false;
    buzzer_process();   // 蜂鸣状态机 10ms 步进
    rgb_process();      // RGB 灯效状态机 10ms 步进
    if (s_prompt_dirty) { s_prompt_dirty=false; board_ui_power(true); ui_render_keypad_entry(); }
    if (s_wake_pending) { s_wake_pending=false; buzzer_beep(BEEP_KEY);
        memset(event,0,sizeof(*event)); event->type=BOARD_TOUCH_WAKE; return true; }
    if (s_menu_active) { /* 菜单模式：逐键上抛 BOARD_MENU_KEY，不喂 keypad_input、不轮询读头 */
        const char key = keypad_scan_step(&s_matrix_io, &s_scan);
        if (key != KEYPAD_CHARACTER_NONE) { buzzer_beep(BEEP_KEY);
            memset(event,0,sizeof(*event)); event->type=BOARD_MENU_KEY;
            event->digits[0]=key; event->digit_count=1U; return true; }
        return false;
    }
    {   /* 键盘 */
        const char key = keypad_scan_step(&s_matrix_io, &s_scan);
        if (key != KEYPAD_CHARACTER_NONE) {
            buzzer_beep(BEEP_KEY);
            if (keypad_input_feed(&s_keypad, key, millis(), event)) {
                if (event->type == BOARD_KEYPAD_CANCEL) { board_ui_power(true); ui_render_keypad_entry(); }
                return true;
            }
            board_ui_power(true); ui_render_keypad_entry();   /* 数字/“*”只进缓冲，给可见回显 */
            memset(event,0,sizeof(*event)); event->type=BOARD_KEYPAD_ACTIVITY; return true;
        }
        if (keypad_input_tick(&s_keypad, millis())) ui_render_keypad_entry();  /* 无操作超时清空 */
    }
    ++s_poll_ticks;
    if ((s_poll_ticks % CARD_POLL_TICKS)==0U)  if (poll_card(event))  return true;   /* 刷卡 */
    if ((s_poll_ticks % FINGER_POLL_TICKS)==0U) if (poll_finger(event)) return true;  /* 指纹 */
    return false;
}''')
bullets([
    "buzzer_process() / rgb_process()：蜂鸣器和 RGB 是“状态机 + 10ms 步进”驱动的，必须在采集节拍里持续喂，"
    "否则蜂鸣/灯效会卡住。这是把外设状态机放进 input 任务的理由。",
    "s_prompt_dirty：应用层改了第 4 行提示语时置位，这里补一次重画（且先 board_ui_power(true) 点屏，"
    "因为 screen 可能已被熄屏/STOP 关掉）。",
    "s_wake_pending：PA8 唤醒键的 EXTI 中断只置这个标志，真正变成 BOARD_TOUCH_WAKE 事件在这里做——"
    "中断里不碰任何应用状态或 FreeRTOS 对象（见 stm32f10x_it.c 注释），绝对安全。",
    "s_menu_active 分支：菜单激活时键盘逐键原样上抛成 BOARD_MENU_KEY，且整体让出读头轮询（指纹/卡），"
    "把总线交给菜单逻辑，避免两套扫描抢同一总线。",
    "keypad_scan_step()：矩阵键盘“一次扫一列”的步进函数，10ms 一拍正好覆盖消抖。",
    "keypad_input_feed()：把单个键喂进 PIN 缓冲，拼满/按 # / 按 B 等才产出 BOARD_AUTH_PIN 等完整事件。",
    "BOARD_KEYPAD_ACTIVITY：纯数字键不产生最终事件，只发“有按键活动”让上层刷新 last_activity_at，"
    "并就地 ui_render_keypad_entry() 做本地回显（否则用户只听到蜂鸣看不到反应）。",
    "poll_card() / poll_finger()：按 CARD_POLL_TICKS/FINGER_POLL_TICKS 分频轮询，不每拍都扫，省总线带宽。",
])
callout("坑：本地回显必须在 input 任务里做",
        "数字键的可见反馈（已输入几位）由板级层直接写 OLED。若交给 ui_task 画，会和菜单/待机画面竞争软 I2C 总线。"
        "同理 board_ui_power(true) 点屏也留在本任务，避免冒出“第三个写 I2C 的任务”破坏事务锁。")

h3("board_diagnostic_report()  —— 诊断输出（board_port.c:446）")
code(
'''void board_diagnostic_report(board_diagnostic_code_t code)
{
    printf("[DIAG] %s\\r\\n", diagnostic_name(code));
}''')
bullets([
    "所有队列满了、存储失败了，统一走这里 printf 到调试串口（USART3 @115200）。",
    "diagnostic_name() 把枚举翻成英文串（如 \"input queue full\"），便于抓日志定位。",
])
callout("坑：诊断只是 printf，不阻塞",
        "队列满时丢弃事件并只打日志——宁可丢一次输入/一条记录，也绝不让低优先级任务卡死高优先级决策。"
        "这是“软降级”而非“硬失败”的设计哲学。")

# ===================================================================
# 第 2 章 access_task
# ===================================================================
h1("第 2 章  access_task（优先级 4，栈 384 字）—— 决策中心")
para("全系统最高优先级（4）。它从 input_queue 取输入事件、从 remote_queue 取云端命令，"
     "做认证决策、驱动锁状态机、处理门磁、到期自动落锁、并在静止 60s 后进 STOP 省电、每 1s 喂看门狗。"
     "它调用了最多的子函数，是整份文档最核心的一章。")

h2("2.1  access_task 函数本体（smart_lock_app.c:1862）")
code(
'''static void access_task(void *argument)
{
    board_input_event_t event;
    remote_command_t command;
    TickType_t last_tick = xTaskGetTickCount();
    (void)argument;
    for (;;)
    {
        if (xQueueReceive(input_queue, &event, pdMS_TO_TICKS(100U)) == pdTRUE)
            handle_board_input(&event, board_unix_time());          /* ① 输入事件 */
        menu_poll(board_unix_time());                                /* ② 菜单周期动作 */
        while (xQueueReceive(remote_queue, &command, 0U) == pdTRUE)  /* ③ 云端命令 */
        {
            const uint64_t now = board_unix_time();
            last_activity_at = now;
            handle_remote_command(&command, now);
            memset(&command, 0, sizeof(command));
        }
        if ((xTaskGetTickCount() - last_tick) >= pdMS_TO_TICKS(1000U))  /* ④ 每秒节拍 */
        {
            const lock_state_t previous = controller.state;
            const uint64_t now = board_unix_time();
            last_tick = xTaskGetTickCount();
            (void)lock_controller_handle(&controller, LOCK_CONTROL_TICK, now);   /* 状态机心跳 */
            if ((pending_first_factor_until != 0U) && (now > pending_first_factor_until))
            { clear_pending_factor(); queue_ui(controller.state, "Second factor expired", true); }
            if (controller.state != previous) {                      /* 状态变了→同步物理+UI+审计 */
                set_physical_state(controller.state);
                queue_ui(controller.state, "Locked", true);
                queue_audit(LOCK_EVENT_LOCK, LOCK_METHOD_SYSTEM, 0U, 1U, now);
            }
            if ((!controller.door_closed) && (!door_ajar_reported) &&
                (door_opened_at != 0U) && (now >= (door_opened_at + DOOR_AJAR_DELAY_SECONDS)))
            {   /* 门虚掩报警 */
                door_ajar_reported = true; board_door_ajar_warning();
                queue_ui(controller.state, "Door is still open", true);
                queue_audit(LOCK_EVENT_DOOR_AJAR, LOCK_METHOD_SYSTEM, 0U, 0U, now);
            }
            if ((controller.state==LOCK_STATE_LOCKED) && controller.door_closed &&
                (pending_first_factor_until==0U) && (!menu_active) &&
                (now >= (last_activity_at + STOP_MODE_DELAY_SECONDS)))
            {   /* 静止 60s 且锁好且无菜单→进 STOP */
                queue_ui(controller.state, NULL, false);
                if (board_try_stop_mode()) { last_activity_at = board_unix_time();
                    queue_ui(controller.state, "System awake", true); }
                else { last_activity_at = now; }
            }
            board_watchdog_refresh();                                /* ⑤ 喂狗 */
        }
    }
}''')
bullets([
    "xQueueReceive(input_queue, 100ms)：阻塞最多 100ms。100ms 既给菜单 poll 兜底周期，又保证按键响应不卡。",
    "menu_poll()：每轮都调（即使没输入），驱动指纹采集步进、等刷卡、菜单无操作超时。",
    "while(...)remote_queue：用 0 超时把队列里所有云端命令一次清空，避免积压。",
    "每秒节拍（last_tick）：所有“时间相关”逻辑（自动落锁、双因子过期、门虚掩、STOP、喂狗）都收敛到 1Hz，"
    "而不是每个事件里各自算时间，时序统一好测。",
    "状态变化检测：先快照 previous，LOCK_CONTROL_TICK 后若 state 变了，才去 set_physical_state + 刷 UI + 记审计。"
    "避免每轮都重复驱动硬件。",
    "STOP 进入条件很苛刻：必须 LOCKED + 门已关 + 无双因子挂起 + 无菜单 + 静止 60s。任一不满足就不睡。",
    "board_watchdog_refresh()：每 1s 喂一次独立看门狗，是系统最后一道保险。",
])
callout("坑：STOP 期间键盘扫描会停",
        "进 STOP 后 MCU 几乎断电，矩阵键盘扫描也停了——所以“菜单激活期间绝不进 STOP”（代码里有 !menu_active 守卫）。"
        "否则菜单会当场死屏。唤醒靠 PA8 外部中断（WFI）。")
callout("坑：双因子过期只清不报错界面",
        "pending_first_factor_until 过期时只 clear_pending_factor + 提示 “Second factor expired”，"
        "不重复落盘（状态没变），因为过期只是内存态失效，无需写 Flash。")

h2("2.2  handle_board_input() —— 认证决策大分派（smart_lock_app.c:1553）")
para("access_task 把每个输入事件交给它。一个 switch 按事件类型分派，是“安全语义”最集中的地方。"
     "下面按 case 拆解关键分支。" )
code(
'''static void handle_board_input(const board_input_event_t *event, uint64_t now)
{
    uint32_t submitted_code = 0U;
    last_activity_at = now;                 /* 任何输入都刷新“最后活动时刻” */
    switch (event->type)
    {   /* —— 键盘 PIN —— */
        case BOARD_AUTH_PIN:
            if (keypad_is_locked(now)) { deny_access(LOCK_METHOD_PIN, id, true, now); }
            else if (pin_credential_matches(&duress_pin, secret, slen, digits, n, true))
                grant_duress(LOCK_METHOD_PIN, id, now);                 /* 胁迫码：静默开+告警 */
            else if (pin_credential_matches(&owner_pin, secret, slen, digits, n, true)) {
                keypad_clear_failures();
                if (second_factor_mode == DISABLED) grant_access(...);
                else if (第一因子已备齐) { clear_pending_factor(); grant_access(第一因子用户, now); }
                else { clear_pending_factor(); deny_access(...); queue_ui("Use first factor first"); }
            }
            else if (visitor_credential_verify_and_consume(&visitor, secret, slen, digits, n, now))
                { keypad_clear_failures(); if (board_security_save(&security_config)) grant_access(VISITOR,id,now);
                  else { 诊断; deny_access(...); } }                    /* 访客一次性码，必须落盘 */
            else deny_access(LOCK_METHOD_PIN, id, keypad_register_failure(now), now);  /* 失败计数 */
            break;
        case BOARD_AUTH_TOTP:               /* 动态口令：纯数字→totp_verify() */
        case BOARD_AUTH_VISITOR:            /* 访客码（刷卡/键盘两种入口） */
        case BOARD_AUTH_FINGERPRINT:        /* 指纹：双因子模式→暂存第一因子；否则直接开 */
        case BOARD_AUTH_DURESS_FINGERPRINT: /* 胁迫指纹→grant_duress */
        case BOARD_AUTH_RFID:               /* 卡：双因子模式→暂存；否则开；管理卡给 30s 免验 */
        case BOARD_AUTH_PERIODIC:           /* 周期密码 */
        case BOARD_DOOR_OPENED / CLOSED:    /* 门磁→lock_controller_handle */
        case BOARD_TAMPER_TRIGGERED:        /* 防拆→lock_controller_handle(TAMPER) */
        case BOARD_MENU_REQUEST:            /* 键盘 'D' 进菜单（锁定/免验期处理） */
        case BOARD_MENU_KEY / CANCEL:       /* 菜单内的键/取消 */
        ...
    }
}''')
bullets([
    "顺序即安全优先级：先判键盘是否被锁 → 再判胁迫 → 业主 → 访客 → 失败计数。胁迫码排最前，"
    "保证被胁迫时也走 grant_duress（静默开门+后台告警），不会被“业主码”分支抢走。",
    "双因子（second_factor_mode）：指纹/卡先来时只把 pending_first_factor 暂存 + 设 30s 超时，"
    "等 PIN 补齐才真正 grant_access。PIN 先来而没第一因子 → 拒绝并提示先刷指纹/卡。",
    "visitor 一次性码：verify_and_consume 成功后必须 board_security_save 落盘，否则重启后这串一次性码又能用一次。",
    "门磁/防拆：直接转交 lock_controller_handle 改状态机，应用层不碰硬件。",
    "BOARD_MENU_REQUEST('D')：锁定期间直接拒绝（防连按 'D' 绕过 60s 锁定）；管理员指纹/管理卡开锁后 30s 内免验进菜单。",
])
callout("坑：访客码曾长期开不了锁（真实 bug）",
        "云端 ISSUE_VISITOR 把临时码好好存进 security_config.visitor，但键盘 PIN 链路的判定顺序里从没校验过它，"
        "导致“临时密码永远开不了锁”。2026-09-14 补上 visitor_credential_verify_and_consume 分支才修好，"
        "且必须排在胁迫/业主之后（撞号时以业主/胁迫语义优先）。")
callout("坑：BOARD_KEYPAD_ACTIVITY 的 case 几乎是空的",
        "它只为让函数开头的 last_activity_at = now 生效——避免用户输入到一半被“静止 60s 熄屏”关屏。"
        "数字键的本地回显已在 input 任务里做，这里不再下 UI 消息，否则会覆盖回显。")

h2("2.3  handle_remote_command() —— 云端命令处理（smart_lock_app.c:1939）")
code(
'''static void handle_remote_command(remote_command_t *command, uint64_t now)
{
    const remote_command_result_t result = remote_command_verify(
        command, security_config.device_secret, sizeof(...), now, 5U,
        &security_config.last_remote_request_id);          /* 验 HMAC + 防重放 */
    if (result != REMOTE_COMMAND_ACCEPTED) {
        queue_audit(LOCK_EVENT_AUTH_FAILURE, LOCK_METHOD_REMOTE, 0U, 0U, now);
        return;                                             /* 验不过直接丢弃 */
    }
    if (command->action == REMOTE_ACTION_ISSUE_VISITOR) {
        if (!visitor_credential_issue(...) || !board_security_save(&security_config))
        { queue_audit(NETWORK,...); return; }
    } else if (!board_security_save(&security_config)) {    /* 重放状态必须先落盘 */
        queue_audit(NETWORK,...); return;
    }
    switch (command->action) {
        case REMOTE_ACTION_UNLOCK:      grant_access(LOCK_METHOD_REMOTE, 0U, now); break;
        case REMOTE_ACTION_LOCK:        lock_controller_handle(&controller, FORCE_LOCK, now);
                                        set_physical_state(controller.state);
                                        queue_audit(LOCK,...); break;
        case REMOTE_ACTION_CLEAR_LOCKOUT: memset(&pin_auth_state,0,...);
                                        lock_controller_handle(&controller, CLEAR_LOCKOUT, now); break;
        case REMOTE_ACTION_ISSUE_VISITOR: queue_audit(NETWORK,...); break;
    }
}''')
bullets([
    "remote_command_verify()：校验云端下发的 HMAC 签名，并用 last_remote_request_id 防重放（5s 窗口）。"
    "验不过一律丢弃，不返回任何状态给攻击者。",
    "先落盘再执行：无论发访客码还是改锁状态，都先把 security_config 存进 Flash，之后才驱动执行机构——"
    "防止“刚执行完就断电、配置没存”导致状态不一致。",
    "grant_access(LOCK_METHOD_REMOTE,...)：远程开锁复用和普通开锁完全相同的开锁路径，审计方法标记为 REMOTE。",
])
callout("坑：落盘失败要中止执行",
        "board_security_save 失败时直接 return 并记审计，绝不继续 grant_access / 改锁——"
        "否则会出现“锁开了但配置没更新”的不可恢复状态。")

h2("2.4  access_task 调用的其它子函数")
h3("queue_ui() —— 投 UI 消息（smart_lock_app.c:240）")
code(
'''static void queue_ui(lock_state_t state, const char *message, bool power_on)
{
    app_ui_message_t ui = { .state=state, .power_on=power_on, .has_log_record=false,
                            .is_menu=false, .menu_page=BOARD_MENU_MAIN, .log_record={0}, .message={0} };
    if (menu_active) return;                  /* 菜单激活时待机/状态消息一律不发，否则顶掉菜单 */
    if (message != NULL) strncpy(ui.message, message, sizeof(ui.message)-1U);
    if (xQueueSend(ui_queue, &ui, 0U) != pdTRUE)
        board_diagnostic_report(BOARD_DIAG_UI_QUEUE_FULL);
}''')
bullets([
    "menu_active 守卫：菜单正在显示时，待机页/状态消息不打进 ui_queue，避免覆盖菜单画面。",
    "xQueueSend 用 0 超时：决策任务不能因 UI 卡住而阻塞（UI 优先级最低，可能正忙）。",
])

h3("queue_audit() —— 投审计 + 通知（smart_lock_app.c:402）")
code(
'''static void queue_audit(lock_event_type_t event_type, lock_auth_method_t method,
                        uint16_t user_id, uint8_t result, uint64_t now)
{
    event_log_record_t record = { .magic=0, .sequence=0, .unix_time=now, .user_id=user_id,
        .event_type=(uint8_t)event_type, .auth_method=(uint8_t)method, .result=result,
        .reserved={0,0,0}, .crc32=0U };
    board_notification_t notification = { .state=controller.state, .event_type=event_type,
        .auth_method=method, .user_id=user_id, .unix_time=now, .result=result };
    if (xQueueSend(log_queue, &record, 0U) != pdTRUE)   board_diagnostic_report(BOARD_DIAG_LOG_QUEUE_FULL);
    if (xQueueSend(notify_queue, &notification, 0U) != pdTRUE) board_diagnostic_report(BOARD_DIAG_NOTIFY_QUEUE_FULL);
}''')
bullets([
    "一条审计事件同时投两份：log_queue（落盘）和 notify_queue（上云）。两份都用 0 超时，队列满就丢并诊断。",
    "record 里 magic/sequence/crc32 先填 0，真正填值在 log_task 调 event_log_append 时算——"
    "职责分离：谁落盘谁填信封。",
])

h3("grant_access / deny_access / grant_duress —— 三个动作收敛点")
code(
'''static void grant_access(lock_auth_method_t method, uint16_t user_id, uint64_t now)
{
    (void)lock_controller_handle(&controller, LOCK_CONTROL_AUTH_GRANTED, now);  /* 状态机→UNLOCKED */
    set_physical_state(controller.state);            /* 驱动硬件开锁 */
    board_auth_feedback(true);                       /* 蜂鸣/灯效“成功” */
    queue_ui(controller.state, "Access granted", true);
    queue_audit(LOCK_EVENT_UNLOCK, method, user_id, 1U, now);
}
static void deny_access(lock_auth_method_t method, uint16_t user_id, bool locked_out, uint64_t now)
{
    if (locked_out) { lock_controller_set_lockout(&controller, pin_auth_state.locked_until);
        set_physical_state(controller.state); board_auth_feedback(false);
        queue_ui(controller.state, "Try again later", true);
        queue_audit(LOCK_EVENT_LOCKOUT, method, user_id, 0U, now); }
    else { board_auth_feedback(false);
        queue_ui(controller.state, "Access denied", true);
        queue_audit(LOCK_EVENT_AUTH_FAILURE, method, user_id, 0U, now); }
}
static void grant_duress(lock_auth_method_t method, uint16_t user_id, uint64_t now)
{
    clear_pending_factor(); keypad_clear_failures();
    (void)lock_controller_handle(&controller, LOCK_CONTROL_AUTH_GRANTED, now);
    set_physical_state(controller.state);
    queue_ui(controller.state, NULL, false);          /* 注意：不提示“已开”，静默 */
    queue_audit(LOCK_EVENT_DURESS, method, user_id, 1U, now);   /* 但审计里记 DURESS */
}''')
bullets([
    "三函数都遵循同一套路：改状态机 → set_physical_state 驱动硬件 → 反馈 → queue_ui → queue_audit。",
    "grant_duress 关键在 queue_ui 传 NULL（不显示“已开锁”，保护被胁迫者），但 queue_audit 记 DURESS，"
    "后台/云端能识别这是胁迫开锁。这就是“静默开门 + 暗告警”。",
    "deny_access 的 locked_out 分支会把状态机切到 LOCKOUT（锁死 60s），普通失败只记审计不锁死。",
])

h3("clear_pending_factor / set_physical_state —— 状态辅助")
code(
'''static void clear_pending_factor(void)
{ pending_first_factor = LOCK_METHOD_SYSTEM; pending_first_factor_user = 0U; pending_first_factor_until = 0U; }
static void set_physical_state(lock_state_t state)
{ board_lock_set(state != LOCK_STATE_UNLOCKED);   /* 非 UNLOCKED 即上锁 */
  board_alarm_set(state == LOCK_STATE_ALARM);     /* 只有 ALARM 才拉报警 */ }''')
bullets([
    "set_physical_state 是“状态机 → 物理世界”的唯一转换点：应用层只改 controller.state，"
    "真正动舵机/蜂鸣由这里翻译。这是全项目最重要的解耦。",
])

h3("lock_controller_handle / lock_controller_set_lockout —— 锁状态机（lock_controller.c）")
code(
'''lock_state_t lock_controller_handle(lock_controller_t *c, lock_control_event_t event, uint64_t now)
{
    if (c == NULL) return LOCK_STATE_ALARM;                 /* 空指针→最安全态 */
    if (event == LOCK_CONTROL_TAMPER) { c->state=LOCK_STATE_ALARM; c->unlock_deadline=0U; return c->state; }
    if ((event==LOCK_CONTROL_CLEAR_ALARM) && (c->state==LOCK_STATE_ALARM)) { c->state=LOCK_STATE_LOCKED; return c->state; }
    if (c->state == LOCK_STATE_ALARM)   return c->state;     /* 报警中无视其它事件 */
    if (c->state == LOCK_STATE_LOCKOUT) {                    /* 锁定中：只等解锁/到期 */
        if ((event==LOCK_CONTROL_CLEAR_LOCKOUT) || ((event==LOCK_CONTROL_TICK)&&(now>=c->lockout_deadline)))
            { c->state=LOCK_STATE_LOCKED; c->lockout_deadline=0U; }
        return c->state;
    }
    switch (event) {
    case LOCK_CONTROL_AUTH_GRANTED: case LOCK_CONTROL_REMOTE_UNLOCK:
        c->state=LOCK_STATE_UNLOCKED; c->unlock_deadline=now+c->auto_lock_seconds; break;
    case LOCK_CONTROL_FORCE_LOCK:      c->state=LOCK_STATE_LOCKED; c->unlock_deadline=0U; break;
    case LOCK_CONTROL_DOOR_OPENED:     c->door_closed=false; break;
    case LOCK_CONTROL_DOOR_CLOSED:     c->door_closed=true;
        if (c->state==LOCK_STATE_UNLOCKED) { c->state=LOCK_STATE_LOCKED; c->unlock_deadline=0U; } break;
    case LOCK_CONTROL_TICK:            /* 到期且门已关→落锁 */
        if ((c->state==LOCK_STATE_UNLOCKED)&&(c->unlock_deadline!=0U)&&(now>=c->unlock_deadline)&&c->door_closed)
            { c->state=LOCK_STATE_LOCKED; c->unlock_deadline=0U; } break;
    default: break;
    }
    return c->state;
}''')
bullets([
    "纯函数式状态机：只改 controller.state，永不直接动硬件——硬件翻译集中在 set_physical_state。",
    "门已开（door_closed=false）时即使到期也不落锁，避免“夹门/夹人”。",
    "ALARM 是最高优先级态：一旦防拆触发，除 CLEAR_ALARM 外所有事件都被忽略。",
])
callout("坑：auto_lock 到期判断必须有 door_closed",
        "LOCK_CONTROL_TICK 的落锁条件带上 c->door_closed——否则门还开着就自动落锁，"
        "舵机顶着门框会堵转/过载。")

h3("board_unix_time / board_door_ajar_warning / board_try_stop_mode / board_watchdog_refresh")
code(
'''uint64_t board_unix_time(void)           /* 读 DS3231 实时时钟，返回 Unix 秒 */
{ DS3231_Time now; DS3231_GetTime(&now); return (uint64_t)DS3231_GetUnix(&now); }
void board_watchdog_refresh(void)        /* 独立看门狗喂狗（IWDG_ReloadCounter） */
{ #if SMART_LOCK_ENABLE_IWDG IWDG_ReloadCounter(); #endif }
void board_door_ajar_warning(void)       /* 门虚掩：连响+报警灯色+串口告警 */
{ buzzer_beep(BEEP_ERR); rgb_set_effect(RGB_EFFECT_ALARM); printf("[ALARM] door left open\\r\\n"); }
bool board_try_stop_mode(void)           /* 进 STOP 前关屏/关射频，唤醒后 SystemInit 回 72M */
{ board_ui_power(false); PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI); SystemInit(); return true; }''')
bullets([
    "board_unix_time 被 access_task 每轮、network_task 拼 token 时调用——它是软 I2C 总线的高频使用者，"
    "所以 I2C 必须加事务级递归锁（见第五章/知识总结文档）。",
    "board_try_stop_mode 唤醒后必须 SystemInit()：STOP 让时钟掉回 HSI，外设时钟需重新配到 72MHz。",
])
callout("坑：STOP 唤醒后不要假设外设还在",
        "进 STOP 后 GPIO/定时器/I2C 状态都可能变，SystemInit() 只恢复时钟树；若某些外设靠板级初始化配置，"
        "需要在唤醒路径里补配。本工程靠 board_init 时钟段 + STOP 前的关屏/关射频把副作用降到最低。")

# ===================================================================
# 第 3 章 network_task
# ===================================================================
h1("第 3 章  network_task（优先级 2，栈 768 字）")
para("联网任务。两件事：① 周期性调 app_wifi_process() 驱动 ESP8266 AT 状态机、把下行桥里收到的云端命令投进 "
     "remote_queue；② 从 notify_queue 取“要上报的事件”，publish 到云平台，失败按指数退避重试。")

h2("3.1  network_task 函数本体（smart_lock_app.c:2004）")
code(
'''static void network_task(void *argument)
{
    remote_command_t command;
    board_notification_t notification;
    (void)argument;
    for (;;)
    {
        if (board_network_poll_command(&command))           /* 驱动 AT 状态机 + 取下行命令 */
        {
            if (xQueueSend(remote_queue, &command, pdMS_TO_TICKS(20U)) != pdTRUE)
                board_diagnostic_report(BOARD_DIAG_REMOTE_QUEUE_FULL);
            memset(&command, 0, sizeof(command));            /* 清零，防残留 */
        }
        if (xQueueReceive(notify_queue, &notification, 0U) == pdTRUE)  /* 上行事件 */
        {
            uint32_t retry_delay_ms = 250U;
            while (!board_network_publish(&notification))    /* 失败指数退避 */
            {
                board_diagnostic_report(BOARD_DIAG_NETWORK_PUBLISH_FAILURE);
                vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
                if (retry_delay_ms < 8000U) retry_delay_ms *= 2U;   /* 250→500→...→8000 封顶 */
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50U));                       /* 50ms 节拍，匹配 AT 状态机设计 */
    }
}''')
bullets([
    "board_network_poll_command 内部调 app_wifi_process()：把“网络心跳”借任务节拍驱动，"
    "且保证下行桥和 AT 状态机在同一任务上下文，无并发竞争。",
    "memset(&command,0,...)：命令投进队列后立刻清零本地副本，避免下次轮询误用残留。",
    "上行 publish 用 while 重试 + 指数退避：250ms 起，每次翻倍，封顶 8s。注意重试期间本任务被 vTaskDelay 阻塞，"
    "但下行 poll 也暂停——这是可接受的折中（离线时上行堆积本就少）。",
    "50ms 节拍：app_wifi 的 push 在离线时静默丢弃，所以 publish 不必自己判在线，直接发即可。",
])
callout("坑：栈必须最大（768 字）",
        "esp8266_mqtt_connect 局部变量约 392B，network 栈曾只有 320 字导致溢出触发“静默死机”（栈溢出钩子不打印）。"
        "实测调到 768 字才稳。这是本项目最经典的栈溢出案例。")

h2("3.2  network_task 调用的子函数")
h3("board_network_poll_command()（board_port.c:827）")
code(
'''bool board_network_poll_command(remote_command_t *command)
{
    app_wifi_process();                                   /* 50~100ms 调一次，驱动 AT 状态机 */
    if ((command==NULL) || (s_remote_head==s_remote_tail)) return false;   /* 环形缓冲空 */
    *command = s_remote_ring[s_remote_tail];
    s_remote_tail = (uint8_t)((s_remote_tail+1U) % REMOTE_REQUEST_RING);   /* 出队 */
    return true;
}''')
bullets([
    "app_wifi_process() 在中断/回调里把云端命令塞进 s_remote_ring 环形缓冲；本任务只是消费者。",
    "环形缓冲 + 单消费者单生产者：因为只在 network_task 一个上下文出队，无需加锁。",
])

h3("board_network_publish()（board_port.c:783）")
code(
'''bool board_network_publish(const board_notification_t *notification)
{
    if (notification==NULL) return false;
    switch (notification->event_type) {
    case LOCK_EVENT_UNLOCK:  app_wifi_push_door_event("unlock", thing_model_method(auth)); app_wifi_push_status(); break;
    case LOCK_EVENT_LOCK:    app_wifi_push_door_event("lock",   thing_model_method(auth)); app_wifi_push_status(); break;
    case LOCK_EVENT_DURESS:  app_wifi_push_duress_event(); app_wifi_push_alarm(ALARM_DURESS); break;
    case LOCK_EVENT_TAMPER:  app_wifi_push_alarm(ALARM_TAMPER); break;
    case LOCK_EVENT_DOOR_AJAR: app_wifi_push_alarm(ALARM_DOOR_AJAR); break;
    case LOCK_EVENT_LOCKOUT:   app_wifi_push_alarm(ALARM_LOCKOUT); break;
    default: break;   /* 物模型无对应属性/事件，平台会拒收，故不上报 */
    }
    return true;
}''')
bullets([
    "thing_model_method()：把本工程的 lock_auth_method_t（0~7）映射到云平台物模型取值（0~5）。"
    "访客/周期密码本质是密码，都映射到 1，否则平台回“参数错误”。",
    "胁迫事件：门照常开，但额外 push_duress_event + alarm，让云端知道这是被胁迫。",
])
callout("坑：物模型 method 越界会吃 2409 错误",
        "平台 method 只接受 0~5，本工程有 0~7。必须显式映射，访客/周期码归到 1（密码），"
        "否则 app_wifi_push 收 2409 参数错误，事件丢失。")

# ===================================================================
# 第 4 章 log_task
# ===================================================================
h1("第 4 章  log_task（优先级 1，栈 256 字）")
para("审计落盘器。从 log_queue 取一条审计记录，写进 W25Q64 的环形日志区；同时应答 log_request_queue 的“查第 N 条”请求，"
     "把记录投回 ui_queue 让 UI 显示。它把所有 Flash 擦写集中在自己一个任务里，避免别处并发写 Flash。")

h2("4.1  log_task 函数本体（smart_lock_app.c:2038）")
code(
'''static void log_task(void *argument)
{
    event_log_record_t record;
    uint32_t offset;
    (void)argument;
    for (;;)
    {
        if (xQueueReceive(log_queue, &record, pdMS_TO_TICKS(50U)) == pdTRUE)
        {
            if (!event_log_append(&event_log, &record))
                board_diagnostic_report(BOARD_DIAG_LOG_STORAGE_FAILURE);   /* 写失败→诊断 */
        }
        while (xQueueReceive(log_request_queue, &offset, 0U) == pdTRUE)   /* 查日志请求 */
        {
            if (event_log_read_recent(&event_log, offset, &record))
                queue_ui_log(&record, true);
            else
                queue_ui_log(NULL, false);                                 /* 没这条→“无记录” */
        }
    }
}''')
bullets([
    "log_queue 用 50ms 超时收：没有写入时让出 CPU，有写入立刻处理。",
    "event_log_append 失败只诊断不重试：审计日志允许丢单条（至少不卡死），但存储故障会被记录便于排查。",
    "log_request_queue 用 0 超时 while 清空：菜单翻页时可能连发多个查记录请求，一次处理完。",
    "queue_ui_log 把查到的记录投给 ui_task；查不到传 NULL + ok=false，UI 显示“无记录”。",
])
callout("坑：Flash 擦写必须集中在一个任务",
        "W25Q64 的擦除（~ms 级）很慢，若在 access/network 任务里直接擦写，会阻塞高优先级决策。"
        "全部收敛到 log_task，其它任务只投 log_queue，天然串行化，无总线竞争。")

h2("4.2  log_task 调用的子函数")
h3("event_log_append() —— 填信封 + 擦 + 写（event_log.c:69）")
code(
'''bool event_log_append(event_log_t *log, event_log_record_t *record)
{
    if ((log==NULL)||(record==NULL)||(!log->initialized)) return false;
    record->magic = EVENT_LOG_MAGIC;
    record->sequence = log->next_sequence;
    memset(record->reserved, 0, sizeof(record->reserved));
    record->crc32 = event_log_crc32(record, offsetof(event_log_record_t, crc32));  /* 算 CRC */
    if (!log->storage.erase(log->storage.context, log->next_slot)) return false;     /* 先擦 */
    if (!log->storage.write(log->storage.context, log->next_slot, record)) return false;  /* 再写 */
    log->next_slot = (log->next_slot+1U) % log->storage.slot_count;                  /* 环形+1 */
    ++log->next_sequence;
    if (log->valid_count < log->storage.slot_count) ++log->valid_count;
    return true;
}''')
bullets([
    "信封（magic/sequence/crc32）在这里填——sender 不必关心持久化细节（职责分离）。",
    "先擦后写：Flash 只能把 1 写 0，改数据必须先整扇区擦成全 1。",
    "环形写：next_slot 取模，写满后覆盖最旧的；sequence 单调递增用于“最近 N 条”和断电恢复判断新旧。",
])

h3("event_log_read_recent() —— 按偏移读最新（event_log.c:96）")
code(
'''bool event_log_read_recent(const event_log_t *log, uint32_t offset, event_log_record_t *record)
{
    if ((log==NULL)||(record==NULL)||(!log->initialized)||(offset>=log->valid_count)) return false;
    uint32_t slot = (log->next_slot + log->storage.slot_count - 1U - offset) % log->storage.slot_count;
    uint32_t expected_sequence = log->next_sequence - 1U - offset;
    return log->storage.read(log->storage.context, slot, record) &&
           event_log_record_valid(record) && (record->sequence == expected_sequence);
}''')
bullets([
    "offset=0 表最新一条，越大越旧。slot 用“next_slot 倒退 offset”算。",
    "读到后还校验 magic+CRC 且 sequence 对得上，保证这条没被写坏/不是旧残影。",
])

h3("board_log_read / write / erase —— Flash 后端（board_port.c:1590）")
code(
'''bool board_log_read(void*ctx,uint32_t slot,event_log_record_t*rec){
    if((rec==NULL)||(slot>=(uint32_t)FLASH_LOG_SLOT_COUNT)) return false;
    W25Q64_Read(log_slot_address(slot),(uint8_t*)rec,(uint16_t)sizeof(*rec)); return true; }
bool board_log_write(void*ctx,uint32_t slot,const event_log_record_t*rec){
    if((rec==NULL)||(slot>=(uint32_t)FLASH_LOG_SLOT_COUNT)) return false;
    W25Q64_PageProgram(log_slot_address(slot),(const uint8_t*)rec,(uint16_t)sizeof(*rec)); return true; }
bool board_log_erase(void*ctx,uint32_t slot){
    if(slot>=(uint32_t)FLASH_LOG_SLOT_COUNT) return false;
    W25Q64_SectorErase(log_slot_address(slot)); return true; }''')
bullets([
    "这三个函数就是 event_log 的“storage”接口实现，通过函数指针表注入，实现纯业务逻辑与片外驱动解耦。",
    "append 先 erase 再过 write；这里 write 只做 PageProgram，保证“断电最多丢这一条”。",
])
callout("坑：event_type 按 uint8_t 持久化",
        "日志结构写进 Flash，新增枚举只能追加到末尾，不能重排或复用旧值，否则旧固件读出错乱。")

# ===================================================================
# 第 5 章 ui_task
# ===================================================================
h1("第 5 章  ui_task（优先级 1，栈 160 字）")
para("最低优先级（与 log 同为 1，但 log 通常更闲）。它用 portMAX_DELAY 永久阻塞等 ui_queue，"
     "收到一帧消息才刷新 OLED——平时完全不占 CPU，屏幕静止时 MCU 可以进 STOP。")

h2("5.1  ui_task 函数本体（smart_lock_app.c:2066）")
code(
'''static void ui_task(void *argument)
{
    app_ui_message_t ui;
    (void)argument;
    for (;;)
    {
        if (xQueueReceive(ui_queue, &ui, portMAX_DELAY) == pdTRUE)   /* 永久阻塞，有消息才醒 */
        {
            board_ui_power(ui.power_on);                             /* 先点/关屏 */
            if (ui.is_menu) {                                        /* 菜单帧 */
                if (ui.menu_page == BOARD_MENU_LOG)
                    board_menu_show_log(ui.has_log_record ? &ui.log_record : NULL);
                else
                    board_menu_show(ui.menu_page, ui.menu_sel, ui.menu_aux,
                                    ui.message[0]!=\\0 ? ui.message : NULL);
            }
            else if (ui.power_on && ui.has_log_record)               /* 菜单外的单条日志帧 */
                board_ui_show_log(&ui.log_record);
            else if (ui.power_on)                                    /* 普通状态帧 */
                board_ui_show(ui.state, ui.message[0]!=\\0 ? ui.message : NULL);
        }
    }
}''')
bullets([
    "portMAX_DELAY：永久阻塞。UI 不主动轮询，完全由别的任务“推”消息驱动——省电且解耦。",
    "board_ui_power(ui.power_on)：每帧先按 power_on 点/关屏。power_on=false 用于“熄屏但不睡”的过渡。",
    "is_menu 分支：菜单页走 board_menu_show（通用列表）或 board_menu_show_log（日志翻页页）；其余走 board_ui_show。",
    "message[0]!='\\0' 判断有没有文本：纯状态刷新（如只画锁图标）可不带 message。",
])
callout("坑：UI 是“推”模型不是“拉”模型",
        "若改成 ui_task 自己去读 controller.state 定时重画，会和 input 任务的本地回显、菜单逻辑抢软 I2C 总线。"
        "全部走 ui_queue 单消费者，配合事务级递归锁，才解决花屏。")

h2("5.2  ui_task 调用的子函数")
h3("board_ui_power()（board_port.c:992）")
code(
'''void board_ui_power(bool enabled) { if (enabled) OLED_DisplayOn(); else OLED_DisplayOff(); }''')
bullets(["最薄的封装：开/关 OLED 显示。关屏不等于断电，只是停止扫描，仍可被唤醒。"])

h3("board_ui_show() —— 待机/状态帧（board_port.c:1123）")
code(
'''void board_ui_show(lock_state_t state, const char *message)
{ ui_render_status(state, message); }   /* 依 state 画锁图标+状态文案+可选 message */''')
bullets([
    "ui_render_status 内部用字库扫描：中文文案按字面量在 oledfont_cn.h 里找点阵，缺字会画空心方框。",
])

h3("board_menu_show() —— 菜单帧（board_port.c:1393）")
code(
'''void board_menu_show(board_menu_page_t page, uint8_t sel, uint8_t aux, const char *line)
{
    switch (page) {
    case BOARD_MENU_MAIN:  menu_draw_list("功能菜单", menu_main_items, ..., sel, aux, "AC选择 D进入","B退出"); break;
    case BOARD_MENU_PIN:   menu_draw_list("密码管理", menu_pin_items, ...); break;
    case BOARD_MENU_FP:    menu_draw_list("指纹管理", menu_fp_items, ...); break;
    case BOARD_MENU_CARD:  menu_draw_list("卡片管理", menu_card_items, ...); break;
    case BOARD_MENU_DURESS:menu_draw_list("胁迫管理", menu_duress_items, ...); break;
    case BOARD_MENU_SYS:   menu_draw_list("系统设置", menu_sys_items, ...); break;
    case BOARD_MENU_COMBO: menu_draw_list("开锁组合", menu_combo_items, ...); break;
    case BOARD_MENU_TIME:  menu_draw_input("修改时间", "YYMMDDHHMMSS", line); break;
    case BOARD_MENU_FP_DEL:menu_draw_input("删除指纹", "编号(1-49)", line); break;
    case BOARD_MENU_INPUT: menu_input_texts(aux, &title, &prompt); menu_draw_input(title, prompt, line); break;
    case BOARD_MENU_FP_ENROLL: menu_draw_fp_enroll(sel, aux, line); break;
    case BOARD_MENU_CARD_WAIT: menu_draw_card_wait(sel, aux, line); break;
    case BOARD_MENU_LOG: default: break;   /* LOG 页走 board_menu_show_log */
    }
}''')
bullets([
    "纯渲染分派：根据 page 选不同菜单项数组 + 提示语（底部 “AC选择 D进入 B退出”）。",
    "line 只允许 ASCII（输入回显、UID 十六进制），中文全部由 board_port.c 按 page/sel/aux 自己取字库——"
    "这样应用层不必关心中文字模，字库改动只动板级层。",
])

h3("board_menu_show_log() / board_ui_show_log() —— 日志帧")
bullets([
    "board_menu_show_log(record)：菜单内“开锁记录”页，底部提示 “AC翻页 B退出”，显示时间+事件+方式。",
    "board_ui_show_log(record)：菜单外的单条日志整屏，时间按 DS3231 转北京时间显示。",
    "两者都用 event_type_text()/auth_method_text() 把枚举翻成中文（开锁/密码/指纹…）。",
])

# ===================================================================
# 第 6 章 跨任务并发要点
# ===================================================================
h1("第 6 章  跨任务协作与并发要点（串起五任务）")
bullets([
    ("队列值拷贝：xQueueSend 把结构体按值拷贝进队列，sender 之后改本地副本不影响队列里的——所以 network_task 投完要 memset 清零只是习惯，并非必须。",0),
    ("优先级：access(4)>input(3)>network(2)>log(1)=ui(1)。最高优先级永远是决策，最低是展示/落盘。",0),
    ("软 I2C 事务锁：OLED(0x78) 与 DS3231(0xD0) 共用 PB8/PB9，被不同优先级任务访问。"
    "在 IIC_WriteBytesRaw 等底层加递归互斥量 s_i2c_mtx，一次 START..STOP 不可抢占——这是解决花屏的关键（见知识总结第①章“递归互斥量”）。",0),
    ("STOP 与 UI：ui_task 用 portMAX_DELAY 阻塞，access_task 在静止 60s 后 board_try_stop_mode() 睡下去，"
    "唤醒靠 PA8 中断；菜单激活时 access 故意不睡，避免键盘扫描停摆。",0),
    ("看门狗：access_task 每 1s 喂 IWDG，是系统最后保险；若某任务死循环卡死调度，1s 内看门狗复位。",0),
    ("栈溢出静默死机：network 栈曾 320 字溢出触发 vApplicationStackOverflowHook（不打印）导致输出冻结；"
    "调到 768 字解决。所有栈用 xTaskCreateStatic 编译期分配，绝不在运行时 malloc。",0),
])

h2("附：五任务创建代码（smart_lock_app.c:2158）")
code(
'''xTaskCreateStatic(input_task,  "input",  INPUT_TASK_STACK_WORDS,  NULL, 3U, input_task_stack,  &input_task_control);
xTaskCreateStatic(access_task,  "access",  ACCESS_TASK_STACK_WORDS,  NULL, 4U, access_task_stack,  &access_task_control);
xTaskCreateStatic(network_task, "network", NETWORK_TASK_STACK_WORDS, NULL, 2U, network_task_stack, &network_task_control);
xTaskCreateStatic(log_task,    "log",     LOG_TASK_STACK_WORDS,     NULL, 1U, log_task_stack,    &log_task_control);
xTaskCreateStatic(ui_task,     "ui",      UI_TASK_STACK_WORDS,      NULL, 1U, ui_task_stack,     &ui_task_control);''')
bullets([
    "xTaskCreateStatic：栈数组和 TCB 都由调用者提供（编译期分配），返回 NULL 表示栈/控制块不够——创建后统一判空。",
    "栈字数：input=256, access=384, network=768, log=256, ui=160（见工作记忆实测配置）。",
])

# 保存
out = r"C:\Users\71563\Desktop\block\docs\五个任务函数详解.docx"
doc.save(out)
print("SAVED =", out)
