#include "smart_lock_app.h"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "as608.h"
#include "board_port.h"
#include "ds3231.h"
#include "event_log.h"
#include "lock_auth.h"
#include "lock_controller.h"
#include "periodic_credential.h"
#include "remote_command.h"
#include "smart_lock_board_config.h"
#include "totp.h"
#include "visitor_credential.h"

#define INPUT_QUEUE_LENGTH 12U
#define REMOTE_QUEUE_LENGTH 4U
#define LOG_QUEUE_LENGTH 8U
#define NOTIFY_QUEUE_LENGTH 8U
#define UI_QUEUE_LENGTH 6U
#define LOG_REQUEST_QUEUE_LENGTH 4U
#define EVENT_LOG_SLOT_COUNT 128U

#define INPUT_TASK_STACK_WORDS 256U
#define ACCESS_TASK_STACK_WORDS 384U
/* 网络任务栈 768 字（= 3072 B），与参考工程 main.c 的 STK_WIFI 一致。
 *
 * 2026-09-14 实测定案：原来只给 320 字（1280 B）会**栈溢出**。
 * esp8266_mqtt_connect() 光局部变量就有 cmd[192] + token[200] = 392 B，
 * 再叠上 onenet_calc_token()（base64/HMAC）与 printf 自身的栈开销就被用穿；
 * 溢出后触发 FreeRTOS 的 vApplicationStackOverflowHook()（关中断 + 死循环，
 * 且不打印任何东西）→ 整机静默死机（键盘/OLED/指纹/刷卡全停，必须断电重启）。
 * 现场特征：串口输出停在 [MQTT] token len=... 这一行中间不动、xTickCount 冻结。 */
#define NETWORK_TASK_STACK_WORDS 768U
#define LOG_TASK_STACK_WORDS 256U
#define UI_TASK_STACK_WORDS 160U

static StaticQueue_t input_queue_control;
static StaticQueue_t remote_queue_control;
static StaticQueue_t log_queue_control;
static StaticQueue_t notify_queue_control;
static StaticQueue_t ui_queue_control;
static StaticQueue_t log_request_queue_control;
static uint8_t input_queue_storage[INPUT_QUEUE_LENGTH * sizeof(board_input_event_t)];
static uint8_t remote_queue_storage[REMOTE_QUEUE_LENGTH * sizeof(remote_command_t)];
static uint8_t log_queue_storage[LOG_QUEUE_LENGTH * sizeof(event_log_record_t)];
static uint8_t notify_queue_storage[NOTIFY_QUEUE_LENGTH * sizeof(board_notification_t)];
typedef struct
{
    lock_state_t state;
    bool power_on;
    bool has_log_record;
    /* 菜单视图（is_menu=1 时 ui 任务改走 board_menu_show()） */
    bool is_menu;
    board_menu_page_t menu_page;
    uint8_t menu_sel;
    uint8_t menu_aux;
    event_log_record_t log_record;
    char message[32]; /* 待机页=状态消息；菜单页=ASCII 动态行（回显/UID） */
} app_ui_message_t;
static uint8_t ui_queue_storage[UI_QUEUE_LENGTH * sizeof(app_ui_message_t)];
static uint8_t log_request_queue_storage[LOG_REQUEST_QUEUE_LENGTH * sizeof(uint32_t)];
static QueueHandle_t input_queue;
static QueueHandle_t remote_queue;
static QueueHandle_t log_queue;
static QueueHandle_t notify_queue;
static QueueHandle_t ui_queue;
static QueueHandle_t log_request_queue;

static StaticTask_t input_task_control;
static StaticTask_t access_task_control;
static StaticTask_t network_task_control;
static StaticTask_t log_task_control;
static StaticTask_t ui_task_control;
static StackType_t input_task_stack[INPUT_TASK_STACK_WORDS];
static StackType_t access_task_stack[ACCESS_TASK_STACK_WORDS];
static StackType_t network_task_stack[NETWORK_TASK_STACK_WORDS];
static StackType_t log_task_stack[LOG_TASK_STACK_WORDS];
static StackType_t ui_task_stack[UI_TASK_STACK_WORDS];

/*
 * 管理菜单（键盘 'D' 进入，2026-09-14 取代原来的"改主密码向导"）。
 *
 * 页面流转（按键习惯照搬参考工程 app_ui.c：A/C 移动、D 进入、B 返回）：
 *
 *   待机 --'D'--> MENU_AUTH（验主密码）--'#'--> MENU_MAIN --D--> 各子菜单
 *                 ▲                             │
 *                 管理员指纹(48页)/管理卡开锁后 30 s 内按 'D' 免验直达主菜单
 *
 *   MENU_MAIN  ─D→ MENU_SUB_PIN    ─D→ 修改主密码（三步输入）/ 胁迫密码
 *              ─D→ MENU_SUB_FP     ─D→ 录入指纹（编号+两按手指）/ 删除 / 管理员(48页)
 *              ─D→ MENU_SUB_CARD   ─D→ 添加卡片 / 删除卡片 / 添加管理卡
 *              ─D→ MENU_SUB_DURESS ─D→ 胁迫密码 / 录胁迫指纹(49页) / 删胁迫指纹
 *              ─D→ MENU_SUB_SYS    ─D→ 修改时间 / 开锁记录 / 开锁组合
 *
 *   菜单/输入页 30 s、等手指/等卡片 60 s 无操作自动退回待机。
 *   菜单激活期间 board_poll_input 不喂 keypad_input、读头也让路
 *   （见 board_menu_set_active），所以这里收到的按键都是 BOARD_MENU_KEY。
 */
typedef enum
{
    MENU_OFF = 0,
    MENU_AUTH,           /* 验证主密码（进菜单的门禁）    */
    MENU_MAIN,           /* 主菜单                        */
    MENU_SUB_PIN,        /* 密码管理                      */
    MENU_SUB_FP,         /* 指纹管理                      */
    MENU_SUB_CARD,       /* 卡片管理                      */
    MENU_SUB_DURESS,     /* 胁迫管理                      */
    MENU_SUB_SYS,        /* 系统设置                      */
    MENU_PIN_OLD,        /* 修改主密码：输原密码          */
    MENU_PIN_NEW,        /* 修改主密码：输新密码          */
    MENU_PIN_CONFIRM,    /* 修改主密码：再输一遍          */
    MENU_DURESS_NEW,     /* 设置胁迫密码                  */
    MENU_DURESS_CONFIRM, /* 设置胁迫密码：再输一遍        */
    MENU_FP_NUM,         /* 录入指纹：输编号(1~47)        */
    MENU_FP_DEL,         /* 删除指纹：输编号(1~49)        */
    MENU_FP_ENROLL,      /* 指纹录入进行中                */
    MENU_CARD_WAIT,      /* 等待刷卡（添加/删除）         */
    MENU_TIME,           /* 修改时间：YYMMDDHHMMSS        */
    MENU_LOG,            /* 开锁记录                      */
    MENU_COMBO           /* 开锁组合（双因子开关）        */
} menu_mode_t;

/* 指纹录入步骤（与 board_port.c 的 menu_draw_fp_enroll 的 aux 约定一致） */
#define FP_STEP_FIRST 0U   /* 请按手指 */
#define FP_STEP_SECOND 1U  /* 再按一次 */
#define FP_STEP_PROCESS 2U /* 处理中   */
#define FP_STEP_DONE 3U    /* 已录入   */
#define FP_STEP_FAIL 4U    /* 录入失败 */

/* 等刷卡状态（与 board_port.c 的 menu_draw_card_wait 的 aux 约定一致） */
#define CARD_WAIT_IDLE 0U /* 请刷卡     */
#define CARD_WAIT_OK 1U   /* 已登记/已删除 */
#define CARD_WAIT_FULL 2U /* 表已满     */
#define CARD_WAIT_NONE 3U /* 卡未登记   */
#define CARD_WAIT_FAIL 4U /* 操作失败   */

/* 指纹录入目标（放在 menu_sel 里携带） */
#define FP_TARGET_NORMAL 0U /* 普通成员：编号 1~47   */
#define FP_TARGET_ADMIN 1U  /* 管理员：固定 48 号页  */
#define FP_TARGET_DURESS 2U /* 胁迫：固定 49 号页    */

#define MENU_TIMEOUT_SECONDS 30U         /* 菜单/输入页无操作回待机 */
#define MENU_CAPTURE_TIMEOUT_SECONDS 60U /* 等手指/等卡片           */
#define MENU_RESULT_SECONDS 2U           /* 操作结果短暂展示        */
#define ADMIN_GRACE_SECONDS 30U          /* 管理员开锁后免验窗口    */

static board_security_config_t security_config;
static lock_auth_state_t pin_auth_state;
static const lock_auth_policy_t pin_policy = {3U, 60U};
static lock_controller_t controller;
static event_log_t event_log;// 最新一条事件日志
static uint64_t door_opened_at;
static bool door_ajar_reported;
static lock_auth_method_t pending_first_factor;
static uint16_t pending_first_factor_user;
static uint64_t pending_first_factor_until;
static uint64_t last_activity_at;

/* ---- 管理菜单（键盘 'D' 进入）的状态，页面流转见上方注释 ---- */
static bool menu_active; /* 菜单是否激活（queue_ui 据此让路）     */
static menu_mode_t menu_mode;
static uint8_t menu_sel;                       /* 菜单高亮项 / 录入目标 / 组合模式      */
static uint8_t menu_status;                    /* 列表页底部的操作结果（BOARD_MENU_STATUS_*）*/
static char menu_input[LOCK_INPUT_MAX_LENGTH]; /* 输入页缓冲             */
static uint8_t menu_input_len;
static char menu_pending[LOCK_PIN_MAX_LENGTH]; /* 修改密码的"第一遍"暂存 */
static uint8_t menu_pending_len;
static uint64_t menu_deadline;     /* 无操作/短暂结果 的截止时刻            */
static bool menu_transient;        /* true=当前帧只是结果展示，到点回父页面 */
static uint64_t admin_grace_until; /* 管理员开锁后免验进菜单的窗口          */
static uint8_t fp_step;            /* 指纹录入步骤（FP_STEP_*）             */
static uint16_t fp_page;           /* 指纹录入目标页号                      */
static uint8_t card_mode;          /* 0 添加普通 1 添加管理卡 2 删除        */
static uint8_t card_status;        /* CARD_WAIT_*                           */
static char card_uid_text[10];     /* 最近刷到卡的 UID 十六进制（ASCII 显示）*/
static uint32_t log_offset;        /* 开锁记录页的翻页游标（0=最新一条）    */

static void handle_remote_command(remote_command_t *command, uint64_t now);
static void keypad_clear_failures(void);
static void set_physical_state(lock_state_t state);
static void queue_audit(lock_event_type_t event_type,
                        lock_auth_method_t method,
                        uint16_t user_id,
                        uint8_t result,
                        uint64_t now);
static void menu_fp_begin(uint8_t target, uint16_t page, uint64_t now);
static void menu_enter_main(uint64_t now);

static bool security_config_valid(const board_security_config_t *config)
{
    uint8_t any_secret = 0U;
    uint8_t any_secret_not_ff = 0U;

    if ((config == NULL) || (!config->owner_pin.valid) ||
        (config->owner_pin.pin_length < LOCK_PIN_MIN_LENGTH) ||
        (config->owner_pin.pin_length > LOCK_PIN_MAX_LENGTH) ||
        (config->totp_secret_length < 16U) ||
        (config->totp_secret_length > BOARD_TOTP_SECRET_MAX_SIZE) ||
        (config->second_factor_mode > BOARD_SECOND_FACTOR_RFID_PIN))
    {
        return false;
    }
    for (size_t i = 0U; i < sizeof(config->device_secret); ++i)
    {
        any_secret |= config->device_secret[i];
        any_secret_not_ff |= (uint8_t)(config->device_secret[i] ^ UINT8_MAX);
    }
    if ((any_secret == 0U) || (any_secret_not_ff == 0U))
    {
        return false;
    }
    if (config->duress_pin.valid &&
        ((config->duress_pin.pin_length < LOCK_PIN_MIN_LENGTH) ||
         (config->duress_pin.pin_length > LOCK_PIN_MAX_LENGTH)))
    {
        return false;
    }
    if (config->periodic_pin.enabled &&
        ((!config->periodic_pin.pin.valid) ||
         (config->periodic_pin.days_mask == 0U) ||
         ((config->periodic_pin.days_mask & UINT8_C(0x80)) != 0U) ||
         (config->periodic_pin.start_minute >=
          config->periodic_pin.end_minute) ||
         (config->periodic_pin.end_minute > 1440U) ||
         (config->periodic_pin.utc_offset_minutes < -720) ||
         (config->periodic_pin.utc_offset_minutes > 840)))
    {
        return false;
    }
    return true;
}

static void queue_ui(lock_state_t state, const char *message, bool power_on)
{
    app_ui_message_t ui = {
        .state = state,
        .power_on = power_on,
        .has_log_record = false,
        .is_menu = false,
        .menu_page = BOARD_MENU_MAIN,
        .menu_sel = 0U,
        .menu_aux = 0U,
        .log_record = {0},
        .message = {0}};
    /* 菜单激活期间，待机页/状态消息一律不发 —— 否则会把正在操作的
     * 菜单画面顶掉。菜单自己的画面走 queue_menu()。 */
    if (menu_active)
    {
        return;
    }
    if (message != NULL)
    {
        (void)strncpy(ui.message, message, sizeof(ui.message) - 1U);
    }
    if (xQueueSend(ui_queue, &ui, 0U) != pdTRUE)
    {
        board_diagnostic_report(BOARD_DIAG_UI_QUEUE_FULL);
    }
}

/*
 * 菜单页视图：ui 任务收到后改走 board_menu_show()。
 *   line 只允许 ASCII（输入回显、UID 十六进制），中文全部由
 *   board_port.c 依据 page/sel/aux 自己取（字库按它的字面量扫描）。
 */
static void queue_menu(board_menu_page_t page, uint8_t sel, uint8_t aux,
                       const char *line)
{
    app_ui_message_t ui = {
        .state = controller.state,
        .power_on = true,
        .has_log_record = false,
        .is_menu = true,
        .menu_page = page,
        .menu_sel = sel,
        .menu_aux = aux,
        .log_record = {0},
        .message = {0}};
    if (line != NULL)
    {
        (void)strncpy(ui.message, line, sizeof(ui.message) - 1U);
    }
    if (xQueueSend(ui_queue, &ui, 0U) != pdTRUE)
    {
        board_diagnostic_report(BOARD_DIAG_UI_QUEUE_FULL);
    }
}

/*
 * 日志一帧：菜单外走老的整屏待机样式；菜单里的"开锁记录"页改走
 * board_menu_show_log()（底部提示是 A/C 翻页、B 退出）。
 * ok=false 表示"没有这条记录"，菜单里显示无记录提示。
 */
static void queue_ui_log(const event_log_record_t *record, bool ok)
{
    app_ui_message_t ui = {
        .state = LOCK_STATE_LOCKED,
        .power_on = true,
        .has_log_record = true,
        .is_menu = false,
        .menu_page = BOARD_MENU_LOG,
        .menu_sel = 0U,
        .menu_aux = 0U,
        .log_record = {0},
        .message = {0}};
    if (menu_active)
    {
        ui.is_menu = true;
        if ((record != NULL) && ok)
        {
            ui.log_record = *record;
        }
        else
        {
            /* 没有这条记录：仍然走 LOG 页，但 has_log_record=false，
             * ui 任务会显示"无记录"提示（见 ui_task）。 */
            ui.has_log_record = false;
        }
    }
    else if ((record != NULL) && ok)
    {
        ui.log_record = *record;
    }
    else
    {
        /* 菜单外且无记录：沿用老的 ASCII 提示 */
        (void)strncpy(ui.message, "No log record", sizeof(ui.message) - 1U);
        ui.has_log_record = false;
    }
    if (xQueueSend(ui_queue, &ui, 0U) != pdTRUE)
    {
        board_diagnostic_report(BOARD_DIAG_UI_QUEUE_FULL);
    }
}

static void clear_pending_factor(void)
{
    pending_first_factor = LOCK_METHOD_SYSTEM;
    pending_first_factor_user = 0U;
    pending_first_factor_until = 0U;
}

static void grant_duress(lock_auth_method_t method, uint16_t user_id,
                         uint64_t now)
{
    clear_pending_factor();
    keypad_clear_failures();
    (void)lock_controller_handle(&controller, LOCK_CONTROL_AUTH_GRANTED, now);
    set_physical_state(controller.state);
    queue_ui(controller.state, NULL, false);
    queue_audit(LOCK_EVENT_DURESS, method, user_id, 1U, now);
}

static bool keypad_is_locked(uint64_t now)
{
    if (now < pin_auth_state.locked_until)
    {
        return true;
    }
    if (pin_auth_state.locked_until != 0U)
    {
        memset(&pin_auth_state, 0, sizeof(pin_auth_state));
    }
    return false;
}

/*
 * 只读版本，供云端模块（network 任务）查询锁定状态。
 * 不做"顺手清除过期锁定"的副作用，避免与 access 任务产生竞争。
 */
bool board_app_is_locked_out(void)
{
    return pin_auth_state.locked_until > board_unix_time();
}

static bool keypad_register_failure(uint64_t now)
{
    if (pin_auth_state.failed_attempts < UINT8_MAX)
    {
        ++pin_auth_state.failed_attempts;
    }
    if (pin_auth_state.failed_attempts >= pin_policy.maximum_failures)
    {
        pin_auth_state.locked_until = now + pin_policy.lockout_seconds;
        return true;
    }
    return false;
}

static void keypad_clear_failures(void)
{
    memset(&pin_auth_state, 0, sizeof(pin_auth_state));
}

static void queue_audit(lock_event_type_t event_type,
                        lock_auth_method_t method,
                        uint16_t user_id,
                        uint8_t result,
                        uint64_t now)
{
    event_log_record_t record = {
        .magic = 0U,
        .sequence = 0U,
        .unix_time = now,
        .user_id = user_id,
        .event_type = (uint8_t)event_type,
        .auth_method = (uint8_t)method,
        .result = result,
        .reserved = {0U, 0U, 0U},
        .crc32 = 0U};
    board_notification_t notification = {
        .state = controller.state,
        .event_type = event_type,
        .auth_method = method,
        .user_id = user_id,
        .unix_time = now,
        .result = result};

    if (xQueueSend(log_queue, &record, 0U) != pdTRUE)
    {
        board_diagnostic_report(BOARD_DIAG_LOG_QUEUE_FULL);
    }
    if (xQueueSend(notify_queue, &notification, 0U) != pdTRUE)
    {
        board_diagnostic_report(BOARD_DIAG_NOTIFY_QUEUE_FULL);
    }
}

static void set_physical_state(lock_state_t state)
{
    board_lock_set(state != LOCK_STATE_UNLOCKED);
    board_alarm_set(state == LOCK_STATE_ALARM);
}

static void grant_access(lock_auth_method_t method, uint16_t user_id,
                         uint64_t now)
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
    if (locked_out)
    {
        lock_controller_set_lockout(&controller, pin_auth_state.locked_until);
        set_physical_state(controller.state);
        board_auth_feedback(false);
        queue_ui(controller.state, "Try again later", true);
        queue_audit(LOCK_EVENT_LOCKOUT, method, user_id, 0U, now);
    }
    else
    {
        board_auth_feedback(false);
        queue_ui(controller.state, "Access denied", true);
        queue_audit(LOCK_EVENT_AUTH_FAILURE, method, user_id, 0U, now);
    }
}

/*
 * ------------------------------------------------------------------
 * 管理菜单（键盘 'D'，2026-09-14 取代原"改主密码向导"）
 * ------------------------------------------------------------------
 * 为什么放在应用层、而不是板级层（沿用原向导的分层理由）：
 *   板级层只认识"按键"，不认识"凭据"。验主密码要拿 HMAC-SHA1 标签做常数时间
 *   比较，落配置要改 security_config 并写 W25Q64 —— 这些状态只有应用层
 *   （而且是 access 任务这一条线程）持有。
 *
 * 为什么菜单入口的密码校验复用 pin_auth_state：
 *   如果另开一套失败计数器，攻击者就能靠连按 'D' 绕开"连续 3 次错误锁定
 *   60 s"的策略。复用 keypad_is_locked / keypad_register_failure 之后，
 *   锁定策略对菜单入口同样生效。
 *
 * 画面走向：本文件只组装"页面 + 序号 + ASCII 动态行"，经 queue_menu() 交给
 * ui 任务调 board_menu_show()；所有中文文案都在 board_port.c（字库扫描约定）。
 */
static const char *menu_echo_text(bool mask)
{
    /* 只在 access 任务里访问，无需加锁 */
    static char buf[LOCK_INPUT_MAX_LENGTH + 1U];

    for (uint8_t i = 0U; i < menu_input_len; ++i)
    {
        buf[i] = mask ? '*' : menu_input[i];
    }
    buf[menu_input_len] = '\0';
    return buf;
}

static void menu_show(void)
{
    static char num[8]; /* 仅 access 任务访问 */

    switch (menu_mode)
    {
    case MENU_AUTH:
        queue_menu(BOARD_MENU_INPUT, 0U, BOARD_MENU_INPUT_AUTH,
                   menu_echo_text(true));
        break;
    case MENU_MAIN:
        queue_menu(BOARD_MENU_MAIN, menu_sel, menu_status, NULL);
        break;
    case MENU_SUB_PIN:
        queue_menu(BOARD_MENU_PIN, menu_sel, menu_status, NULL);
        break;
    case MENU_SUB_FP:
        queue_menu(BOARD_MENU_FP, menu_sel, menu_status, NULL);
        break;
    case MENU_SUB_CARD:
        queue_menu(BOARD_MENU_CARD, menu_sel, menu_status, NULL);
        break;
    case MENU_SUB_DURESS:
        queue_menu(BOARD_MENU_DURESS, menu_sel, menu_status, NULL);
        break;
    case MENU_SUB_SYS:
        queue_menu(BOARD_MENU_SYS, menu_sel, menu_status, NULL);
        break;
    case MENU_PIN_OLD:
        queue_menu(BOARD_MENU_INPUT, 0U, BOARD_MENU_INPUT_OLD_PIN,
                   menu_echo_text(true));
        break;
    case MENU_PIN_NEW:
        queue_menu(BOARD_MENU_INPUT, 0U, BOARD_MENU_INPUT_NEW_PIN,
                   menu_echo_text(true));
        break;
    case MENU_PIN_CONFIRM:
        queue_menu(BOARD_MENU_INPUT, 0U, BOARD_MENU_INPUT_CONFIRM,
                   menu_echo_text(true));
        break;
    case MENU_DURESS_NEW:
        queue_menu(BOARD_MENU_INPUT, 0U, BOARD_MENU_INPUT_DURESS,
                   menu_echo_text(true));
        break;
    case MENU_DURESS_CONFIRM:
        queue_menu(BOARD_MENU_INPUT, 0U, BOARD_MENU_INPUT_DURESS_CONFIRM,
                   menu_echo_text(true));
        break;
    case MENU_FP_NUM:
        queue_menu(BOARD_MENU_INPUT, 0U, BOARD_MENU_INPUT_FP_NUM,
                   menu_echo_text(false));
        break;
    case MENU_FP_ENROLL:
        (void)snprintf(num, sizeof(num), "%u", (unsigned)fp_page);
        queue_menu(BOARD_MENU_FP_ENROLL, menu_sel, fp_step, num);
        break;
    case MENU_CARD_WAIT:
        queue_menu(BOARD_MENU_CARD_WAIT, card_mode, card_status, card_uid_text);
        break;
    case MENU_TIME:
        queue_menu(BOARD_MENU_TIME, 0U, 0U, menu_echo_text(false));
        break;
    case MENU_COMBO:
        queue_menu(BOARD_MENU_COMBO, menu_sel, menu_status, NULL);
        break;
    case MENU_LOG:
    default:
        /* 日志页由 log_task 的应答驱动（queue_ui_log 的菜单分支），这里不画 */
        break;
    }
}

/* 切换到某个菜单页（清输入缓冲、刷新超时） */
static void menu_goto(menu_mode_t mode, uint64_t now)
{
    menu_mode = mode;
    menu_input_len = 0U;
    (void)memset(menu_input, 0, sizeof(menu_input));
    menu_status = BOARD_MENU_STATUS_NONE;
    menu_transient = false;
    menu_deadline = now + ((mode == MENU_FP_ENROLL) || (mode == MENU_CARD_WAIT)
                               ? MENU_CAPTURE_TIMEOUT_SECONDS
                               : MENU_TIMEOUT_SECONDS);
    if (mode == MENU_FP_ENROLL)
    {
        fp_step = FP_STEP_FIRST;
    }
    if (mode == MENU_CARD_WAIT)
    {
        card_status = CARD_WAIT_IDLE;
        card_uid_text[0] = '\0';
    }
    menu_show();
}

/* 操作结果：回到父页面并显示 status，MENU_RESULT_SECONDS 后自动停留 */
static void menu_result(menu_mode_t back, uint8_t status, uint64_t now)
{
    menu_mode = back;
    menu_status = status;
    menu_transient = true;
    menu_deadline = now + MENU_RESULT_SECONDS;
    menu_show();
}

/* 完全退出菜单，恢复待机整屏 */
static void menu_exit(void)
{
    menu_active = false;
    board_menu_set_active(false);
    menu_mode = MENU_OFF;
    menu_input_len = 0U;
    menu_pending_len = 0U;
    (void)memset(menu_input, 0, sizeof(menu_input));
    (void)memset(menu_pending, 0, sizeof(menu_pending));
    menu_transient = false;
    menu_status = BOARD_MENU_STATUS_NONE;
    queue_ui(controller.state, NULL, true);
}

/* 开机后/收尾时复位菜单状态（不发画面） */
static void menu_reset(void)
{
    menu_active = false;
    board_menu_set_active(false);
    menu_mode = MENU_OFF;
    menu_sel = 0U;
    menu_status = BOARD_MENU_STATUS_NONE;
    menu_input_len = 0U;
    menu_pending_len = 0U;
    (void)memset(menu_input, 0, sizeof(menu_input));
    (void)memset(menu_pending, 0, sizeof(menu_pending));
    menu_deadline = 0U;
    menu_transient = false;
    admin_grace_until = 0U;
    fp_step = FP_STEP_FIRST;
    fp_page = 0U;
    card_mode = 0U;
    card_status = CARD_WAIT_IDLE;
    card_uid_text[0] = '\0';
    log_offset = 0U;
}

/* 菜单门禁：验主密码（'#' 提交） */
static void menu_auth_submit(uint64_t now)
{
    if (pin_credential_matches(&security_config.owner_pin,
                               security_config.device_secret,
                               sizeof(security_config.device_secret),
                               menu_input, menu_input_len, false))
    {
        keypad_clear_failures();
        board_auth_feedback(true);
        menu_enter_main(now);
        return;
    }
    /* 密码错：与普通登录同一条失败路径（计数 + 锁定） */
    board_auth_feedback(false);
    deny_access(LOCK_METHOD_PIN, 0U, keypad_register_failure(now), now);
    if (keypad_is_locked(now))
    {
        menu_exit();
        queue_ui(controller.state, "Try again later", true);
        return;
    }
    menu_input_len = 0U;
    menu_input[0] = '\0';
    menu_show();
}

/* 修改主密码 / 胁迫密码：'#' 提交（各步逻辑沿用原向导，含先比长度再 memcmp） */
static void menu_pin_submit(uint64_t now)
{
    switch (menu_mode)
    {
    case MENU_PIN_OLD:
        if (!pin_credential_matches(&security_config.owner_pin,
                                    security_config.device_secret,
                                    sizeof(security_config.device_secret),
                                    menu_input, menu_input_len, false))
        {
            board_auth_feedback(false);
            deny_access(LOCK_METHOD_PIN, 0U, keypad_register_failure(now), now);
            if (keypad_is_locked(now))
            {
                menu_exit();
                queue_ui(controller.state, "Try again later", true);
                return;
            }
            menu_input_len = 0U;
            menu_input[0] = '\0';
            menu_show();
            return;
        }
        keypad_clear_failures();
        board_auth_feedback(true);
        menu_goto(MENU_PIN_NEW, now);
        return;

    case MENU_PIN_NEW:
        if ((menu_input_len < (uint8_t)LOCK_PIN_MIN_LENGTH) ||
            (menu_input_len > (uint8_t)LOCK_PIN_MAX_LENGTH))
        {
            /* 长度不合规：留在本页重输 */
            board_auth_feedback(false);
            menu_show();
            return;
        }
        (void)memcpy(menu_pending, menu_input, (size_t)menu_input_len);
        menu_pending_len = menu_input_len;
        board_auth_feedback(true);
        menu_goto(MENU_PIN_CONFIRM, now);
        return;

    case MENU_PIN_CONFIRM:
        /* ⚠ 先比长度再 memcmp（menu_pending 只有 LOCK_PIN_MAX_LENGTH 字节），
         *   靠 || 短路避免越界读 —— 与原向导相同的约束。 */
        if ((menu_input_len != menu_pending_len) ||
            (memcmp(menu_input, menu_pending, (size_t)menu_input_len) != 0))
        {
            menu_pending_len = 0U;
            (void)memset(menu_pending, 0, sizeof(menu_pending));
            board_auth_feedback(false);
            menu_goto(MENU_PIN_NEW, now);
            return;
        }
        {
            /* 先在副本上算新标签，落盘成功才认账（与原向导一致） */
            const pin_credential_t original = security_config.owner_pin;
            pin_credential_t updated = original;

            if (pin_credential_set(&updated,
                                   security_config.device_secret,
                                   sizeof(security_config.device_secret),
                                   menu_pending, menu_pending_len))
            {
                security_config.owner_pin = updated;
                if (board_security_save(&security_config))
                {
                    queue_audit(LOCK_EVENT_CONFIG_CHANGE, LOCK_METHOD_SYSTEM,
                                0U, 1U, now);
                    board_auth_feedback(true);
                    menu_result(MENU_SUB_PIN, BOARD_MENU_STATUS_SAVED, now);
                    return;
                }
                security_config.owner_pin = original; /* 落盘失败 → 回滚 */
            }
            board_diagnostic_report(BOARD_DIAG_SECURITY_STORAGE_FAILURE);
            board_auth_feedback(false);
            menu_result(MENU_SUB_PIN, BOARD_MENU_STATUS_FAIL, now);
        }
        return;

    case MENU_DURESS_NEW:
        if ((menu_input_len < (uint8_t)LOCK_PIN_MIN_LENGTH) ||
            (menu_input_len > (uint8_t)LOCK_PIN_MAX_LENGTH))
        {
            board_auth_feedback(false);
            menu_show();
            return;
        }
        (void)memcpy(menu_pending, menu_input, (size_t)menu_input_len);
        menu_pending_len = menu_input_len;
        board_auth_feedback(true);
        menu_goto(MENU_DURESS_CONFIRM, now);
        return;

    case MENU_DURESS_CONFIRM:
        if ((menu_input_len != menu_pending_len) ||
            (memcmp(menu_input, menu_pending, (size_t)menu_input_len) != 0))
        {
            menu_pending_len = 0U;
            (void)memset(menu_pending, 0, sizeof(menu_pending));
            board_auth_feedback(false);
            menu_goto(MENU_DURESS_NEW, now);
            return;
        }
        {
            const pin_credential_t original = security_config.duress_pin;
            pin_credential_t updated = original;

            if (pin_credential_set(&updated,
                                   security_config.device_secret,
                                   sizeof(security_config.device_secret),
                                   menu_pending, menu_pending_len))
            {
                /* 胁迫密码不允许与主密码相同：否则胁迫语义失效 */
                if (pin_credential_matches(&security_config.owner_pin,
                                           security_config.device_secret,
                                           sizeof(security_config.device_secret),
                                           menu_pending, menu_pending_len,
                                           false))
                {
                    board_auth_feedback(false);
                    menu_result(MENU_SUB_DURESS, BOARD_MENU_STATUS_DUP, now);
                    return;
                }
                security_config.duress_pin = updated;
                if (board_security_save(&security_config))
                {
                    queue_audit(LOCK_EVENT_CONFIG_CHANGE, LOCK_METHOD_SYSTEM,
                                0U, 1U, now);
                    board_auth_feedback(true);
                    menu_result(MENU_SUB_DURESS, BOARD_MENU_STATUS_SAVED, now);
                    return;
                }
                security_config.duress_pin = original; /* 落盘失败 → 回滚 */
            }
            board_diagnostic_report(BOARD_DIAG_SECURITY_STORAGE_FAILURE);
            board_auth_feedback(false);
            menu_result(MENU_SUB_DURESS, BOARD_MENU_STATUS_FAIL, now);
        }
        return;

    case MENU_FP_NUM: {
        /* 编号 1~47（48=管理员、49=胁迫 由专门入口管理） */
        uint32_t num = 0U;

        if (menu_input_len == 0U)
        {
            board_auth_feedback(false);
            menu_show();
            return;
        }
        for (uint8_t i = 0U; i < menu_input_len; ++i)
        {
            num = (num * 10U) + (uint32_t)(menu_input[i] - '0');
        }
        if ((num < 1U) || (num > 47U))
        {
            board_auth_feedback(false);
            menu_input_len = 0U;
            menu_input[0] = '\0';
            menu_show();
            return;
        }
        board_auth_feedback(true);
        menu_fp_begin(FP_TARGET_NORMAL, (uint16_t)num, now);
        return;
    }

    case MENU_TIME: {
        /* 12 位 YYMMDDHHMMSS；年=20YY。星期由日期反算。
         * 注意：系统内部按 UTC 存（DS3231_BUILD_TZ_HOURS=8，GetUnix 会扣掉
         * 8 小时），所以这里输入北京时间即可，与 TOTP/云端时间轴一致。 */
        DS3231_Time t;
        uint32_t v[6];

        if (menu_input_len != 12U)
        {
            board_auth_feedback(false);
            menu_show();
            return;
        }
        for (uint8_t i = 0U; i < 6U; ++i)
        {
            v[i] = ((uint32_t)(menu_input[i * 2U] - '0') * 10U) +
                   (uint32_t)(menu_input[(i * 2U) + 1U] - '0');
        }
        if ((v[0] > 99U) || (v[1] < 1U) || (v[1] > 12U) ||
            (v[2] < 1U) || (v[2] > 31U) ||
            (v[3] > 23U) || (v[4] > 59U) || (v[5] > 59U))
        {
            board_auth_feedback(false);
            menu_input_len = 0U;
            menu_input[0] = '\0';
            menu_show();
            return;
        }
        t.year = (uint8_t)v[0];
        t.month = (uint8_t)v[1];
        t.date = (uint8_t)v[2];
        t.hour = (uint8_t)v[3];
        t.min = (uint8_t)v[4];
        t.sec = (uint8_t)v[5];
        /* 输入的 12 位是**北京时间**；芯片与内部时间轴一律 UTC
         * （TOTP / OneNET token / 锁定时效都按 UTC 算），所以先做
         * 本地→UTC 换算，再用 UTC 反算日历字段（含星期 —— 跨零点的
         * 时区换算会改变日期与星期，不能让用户自己填）。 */
        DS3231_UnixToTime(DS3231_LocalTimeToUtcUnix(&t), &t);
        DS3231_SetTime(&t);
        queue_audit(LOCK_EVENT_CONFIG_CHANGE, LOCK_METHOD_SYSTEM, 0U, 1U, now);
        board_auth_feedback(true);
        menu_result(MENU_SUB_SYS, BOARD_MENU_STATUS_SAVED, now);
        return;
    }

    default:
        break;
    }
}

/* 指纹录入起点：设定目标页并进入采集流程 */
static void menu_fp_begin(uint8_t target, uint16_t page, uint64_t now)
{
    fp_page = page;
    menu_sel = target;
    menu_goto(MENU_FP_ENROLL, now);
}

/* 指纹录入失败：短暂展示后回指纹管理 */
static void menu_fp_fail(uint64_t now)
{
    fp_step = FP_STEP_FAIL;
    menu_transient = true;
    menu_status = BOARD_MENU_STATUS_FAIL;
    menu_deadline = now + MENU_RESULT_SECONDS;
    board_auth_feedback(false);
    menu_show();
}

static void menu_enter_main(uint64_t now);
static void menu_key(char key, uint64_t now);

/* 进入"验主密码"页（菜单门禁第一步） */
static void menu_begin_auth(uint64_t now)
{
    menu_active = true;
    board_menu_set_active(true);
    menu_mode = MENU_AUTH;
    menu_input_len = 0U;
    (void)memset(menu_input, 0, sizeof(menu_input));
    menu_transient = false;
    menu_status = BOARD_MENU_STATUS_NONE;
    menu_deadline = now + MENU_TIMEOUT_SECONDS;
    menu_show();
}

/* 验证通过 → 主菜单 */
static void menu_enter_main(uint64_t now)
{
    menu_active = true;
    board_menu_set_active(true);
    menu_mode = MENU_MAIN;
    menu_sel = 0U;
    menu_status = BOARD_MENU_STATUS_NONE;
    menu_transient = false;
    menu_deadline = now + MENU_TIMEOUT_SECONDS;
    board_auth_feedback(true);
    menu_show();
}

/* 菜单状态下每轮（≈100 ms）轮询：指纹采集、等卡、超时 */
static void menu_poll(uint64_t now)
{
    // 菜单未激活，直接返回
    if (!menu_active)
    {
        return;
    }
    /* 结果展示帧：到点回父页面 */
    if (menu_transient && (now >= menu_deadline))
    {
        menu_transient = false;
        menu_status = BOARD_MENU_STATUS_NONE;
        menu_deadline = now + MENU_TIMEOUT_SECONDS;
        menu_show();
        return;
    }
    /* 无操作超时（30 s 菜单 / 60 s 等待手指卡片） */
    if ((!menu_transient) && (now >= menu_deadline))
    {
        menu_exit();
        return;
    }

    switch (menu_mode)
    {
    // 指纹录入
    case MENU_FP_ENROLL: {
        int r;

        if ((fp_step == FP_STEP_DONE) || (fp_step == FP_STEP_FAIL))
        {
            break; /* 结果帧由上面的 transient 逻辑收尾 */
        }
        r = AS608_GetImage(); // 获取指纹图像
        if (r < 0)
        {
            menu_fp_fail(now); /* 通信失败 */
            break;
        }
        if (r == 2)
        {
            break; /* 0x02 = 无手指，继续等 */
        }
        if (r != 0)
        {
            break; /* 其他确认码（图像质量差等）：继续等 */
        }
        // 根据采集到的图像，提取指纹特征点
        r = AS608_GenChar((fp_step == FP_STEP_FIRST) ? 1U : 2U);
        if (r != 0)
        {
            menu_fp_fail(now); /* 生成特征失败 */
            break;
        }
        // 第一次采集，注册模板
        if (fp_step == FP_STEP_FIRST)
        {
            fp_step = FP_STEP_SECOND;
            menu_deadline = now + MENU_CAPTURE_TIMEOUT_SECONDS;
            menu_show();
        }
        // 第二次采集，存储模板
        else
        {
            fp_step = FP_STEP_PROCESS;
            menu_show();
            r = AS608_RegModel();
            if (r == 0)
            {
                r = AS608_StoreChar(1U, fp_page);
            }
            if (r == 0)
            {
                fp_step = FP_STEP_DONE;
                menu_transient = true;
                menu_status = BOARD_MENU_STATUS_OK;
                menu_deadline = now + MENU_RESULT_SECONDS;
                board_auth_feedback(true);
                queue_audit(LOCK_EVENT_ENROLL, LOCK_METHOD_SYSTEM,
                            (uint16_t)fp_page, 1U, now);
            }
            else
            {
                menu_fp_fail(now);
            }
            menu_show();
        }
        break;
    }
    // 等待卡片
    case MENU_CARD_WAIT: {
        uint8_t uid[FLASH_CARD_UID_SIZE];

        if (!board_card_read(uid))
        {
            break; /* 还没有新卡 */
        }
        (void)snprintf(card_uid_text, sizeof(card_uid_text), "%02X%02X%02X%02X",
                       uid[0], uid[1], uid[2], uid[3]);
        // 删除卡片
        if (card_mode == 2U)
        {
            // 在 Flash 卡片表查找并删除该 UID 记录
            if (board_card_remove(uid))
            {
                card_status = CARD_WAIT_OK;
                menu_status = BOARD_MENU_STATUS_OK;
                queue_audit(LOCK_EVENT_DELETE, LOCK_METHOD_SYSTEM, 0U, 1U, now); // 记录删除事件
                board_auth_feedback(true);                                       // 删除成功
            }
            else
            {
                card_status = CARD_WAIT_NONE;
                menu_status = BOARD_MENU_STATUS_NOENT; // 未找到该卡片
                board_auth_feedback(false);            // 删除失败
            }
        }
        // 注册卡片
        else
        {
            const uint8_t role = (card_mode == 1U) ? BOARD_CARD_ROLE_ADMIN   // 注册管理员
                                                   : BOARD_CARD_ROLE_NORMAL; // 注册普通成员
            const uint16_t user_id = (uint16_t)(board_card_count() + 1U);

            // 把 UID、角色、用户 ID 写入 Flash 卡片存储表
            // 如果成功，返回 true；否则返回 false
            if (board_card_add(uid, role, user_id))
            {
                card_status = CARD_WAIT_OK;
                menu_status = BOARD_MENU_STATUS_OK;
                queue_audit(LOCK_EVENT_ENROLL, LOCK_METHOD_SYSTEM, user_id, 1U, now); // 记录注册事件
                board_auth_feedback(true);                                            // 注册成功
            }
            // 卡片表已满
            else if (board_card_count() >=
                     (uint8_t)FLASH_CARD_TABLE_CAPACITY)
            {
                card_status = CARD_WAIT_FULL;
                menu_status = BOARD_MENU_STATUS_FULL;
                board_auth_feedback(false);
            }
            // 添加失败
            else
            {
                card_status = CARD_WAIT_FAIL;
                menu_status = BOARD_MENU_STATUS_DUP;
                board_auth_feedback(false);
            }
        }
        /* 结果短暂展示后回卡片管理（transient 逻辑收尾） */
        menu_transient = true;
        menu_deadline = now + MENU_RESULT_SECONDS;
        menu_show();
        break;
    }

    default:
        break;
    }
}

/* 输入类页面的 '#' 提交 */
static void menu_submit(uint64_t now)
{
    switch (menu_mode)
    {
    case MENU_AUTH:
        menu_auth_submit(now);
        break;
    case MENU_PIN_OLD:
    case MENU_PIN_NEW:
    case MENU_PIN_CONFIRM:
    case MENU_DURESS_NEW:
    case MENU_DURESS_CONFIRM:
    case MENU_FP_NUM:
    case MENU_TIME:
        menu_pin_submit(now);
        break;
    default:
        break;
    }
}

/* 输入类页面的 'C'：退回父菜单 */
static void menu_cancel_page(uint64_t now)
{
    switch (menu_mode)
    {
    case MENU_AUTH:
        menu_exit();
        break;
    case MENU_PIN_OLD:
    case MENU_PIN_NEW:
    case MENU_PIN_CONFIRM:
        menu_goto(MENU_SUB_PIN, now);
        break;
    case MENU_DURESS_NEW:
    case MENU_DURESS_CONFIRM:
        menu_goto(MENU_SUB_DURESS, now);
        break;
    case MENU_FP_NUM:
        menu_goto(MENU_SUB_FP, now);
        break;
    case MENU_TIME:
        menu_goto(MENU_SUB_SYS, now);
        break;
    default:
        menu_exit();
        break;
    }
}

/* 开锁记录页：A=更旧一条 C=更新一条 B=退出 */
static void menu_log_key(char key, uint64_t now)
{
    if (key == 'A')
    {
        ++log_offset;
    }
    else if ((key == 'C') && (log_offset > 0U))
    {
        --log_offset;
    }
    else if (key == 'B')
    {
        menu_goto(MENU_SUB_SYS, now);
        return;
    }
    else
    {
        return;
    }
    if (xQueueSend(log_request_queue, &log_offset, 0U) != pdTRUE)
    {
        board_diagnostic_report(BOARD_DIAG_LOG_QUEUE_FULL);
    }
}

/* 菜单按键总入口 */
static void menu_key(char key, uint64_t now)
{
    /* 结果展示帧：任意键清掉状态继续操作 */
    if (menu_transient)
    {
        menu_transient = false;
        menu_status = BOARD_MENU_STATUS_NONE;
        menu_deadline = now + MENU_TIMEOUT_SECONDS;
        menu_show();
        return;
    }

    menu_deadline = now + ((menu_mode == MENU_FP_ENROLL) ||
                                   (menu_mode == MENU_CARD_WAIT)
                               ? MENU_CAPTURE_TIMEOUT_SECONDS
                               : MENU_TIMEOUT_SECONDS);

    switch (menu_mode)
    {
    /* ---------- 输入类页面：数字 / B 删一位 / # 确认 / C 退出 ---------- */
    case MENU_AUTH:
    case MENU_PIN_OLD:
    case MENU_PIN_NEW:
    case MENU_PIN_CONFIRM:
    case MENU_DURESS_NEW:
    case MENU_DURESS_CONFIRM:
    case MENU_FP_NUM:
    case MENU_TIME:
        if ((key >= '0') && (key <= '9'))
        {
            if (menu_input_len < LOCK_INPUT_MAX_LENGTH)
            {
                menu_input[menu_input_len] = key;
                ++menu_input_len;
            }
        }
        else if (key == 'B')
        {
            if (menu_input_len > 0U)
            {
                --menu_input_len;
                menu_input[menu_input_len] = '\0';
            }
        }
        else if (key == '#')
        {
            menu_submit(now);
            return;
        }
        else if (key == 'C')
        {
            menu_cancel_page(now);
            return;
        }
        menu_show();
        break;

    /* ---------- 主菜单 ---------- */
    case MENU_MAIN:
        if (key == 'A')
        {
            menu_sel = (uint8_t)((menu_sel + 4U) % 5U);
        }
        else if (key == 'C')
        {
            menu_sel = (uint8_t)((menu_sel + 1U) % 5U);
        }
        else if (key == 'D')
        {
            static const menu_mode_t subs[5] = {
                MENU_SUB_PIN, MENU_SUB_FP, MENU_SUB_CARD,
                MENU_SUB_DURESS, MENU_SUB_SYS};
            menu_goto(subs[menu_sel], now);
            return;
        }
        else if (key == 'B')
        {
            menu_exit();
            return;
        }
        menu_show();
        break;

    /* ---------- 密码管理（2 项） ---------- */
    case MENU_SUB_PIN:
        if (key == 'A')
        {
            menu_sel = (uint8_t)(menu_sel ^ 1U);
        }
        else if (key == 'C')
        {
            menu_sel = (uint8_t)(menu_sel ^ 1U);
        }
        else if (key == 'D')
        {
            if (menu_sel == 0U)
            {
                menu_goto(MENU_PIN_OLD, now);
            }
            else
            {
                menu_goto(MENU_DURESS_NEW, now);
            }
            return;
        }
        else if (key == 'B')
        {
            menu_goto(MENU_MAIN, now);
            return;
        }
        menu_show();
        break;

    /* ---------- 指纹管理（3 项） ---------- */
    case MENU_SUB_FP:
        if (key == 'A')
        {
            menu_sel = (uint8_t)((menu_sel + 2U) % 3U);
        }
        else if (key == 'C')
        {
            menu_sel = (uint8_t)((menu_sel + 1U) % 3U);
        }
        else if (key == 'D')
        {
            if (menu_sel == 0U)
            {
                menu_goto(MENU_FP_NUM, now);
            }
            else if (menu_sel == 1U)
            {
                menu_goto(MENU_FP_DEL, now);
            }
            else
            {
                /* 管理员指纹 = 固定 48 号页 */
                menu_fp_begin(FP_TARGET_ADMIN, (uint16_t)48U, now);
            }
            return;
        }
        else if (key == 'B')
        {
            menu_goto(MENU_MAIN, now);
            return;
        }
        menu_show();
        break;

    /* ---------- 卡片管理（3 项） ---------- */
    case MENU_SUB_CARD:
        if (key == 'A')
        {
            menu_sel = (uint8_t)((menu_sel + 2U) % 3U);
        }
        else if (key == 'C')
        {
            menu_sel = (uint8_t)((menu_sel + 1U) % 3U);
        }
        else if (key == 'D')
        {
            card_mode = menu_sel; /* 0 添加普通 1 添加管理卡 2 删除 */
            menu_goto(MENU_CARD_WAIT, now);
            return;
        }
        else if (key == 'B')
        {
            menu_goto(MENU_MAIN, now);
            return;
        }
        menu_show();
        break;

    /* ---------- 胁迫管理（3 项） ---------- */
    case MENU_SUB_DURESS:
        if (key == 'A')
        {
            menu_sel = (uint8_t)((menu_sel + 2U) % 3U);
        }
        else if (key == 'C')
        {
            menu_sel = (uint8_t)((menu_sel + 1U) % 3U);
        }
        else if (key == 'D')
        {
            if (menu_sel == 0U)
            {
                menu_goto(MENU_DURESS_NEW, now);
            }
            else if (menu_sel == 1U)
            {
                /* 胁迫指纹 = 固定 49 号页 */
                menu_fp_begin(FP_TARGET_DURESS, (uint16_t)49U, now);
            }
            else
            {
                /* 删除胁迫指纹：直接删 49 页 */
                if (AS608_DeleteChar((uint16_t)AS608_DURESS_PAGE, 1U) == 0)
                {
                    queue_audit(LOCK_EVENT_DELETE, LOCK_METHOD_SYSTEM,
                                (uint16_t)AS608_DURESS_PAGE, 1U, now);
                    board_auth_feedback(true);
                    menu_result(MENU_SUB_DURESS, BOARD_MENU_STATUS_OK, now);
                }
                else
                {
                    board_auth_feedback(false);
                    menu_result(MENU_SUB_DURESS, BOARD_MENU_STATUS_FAIL, now);
                }
            }
            return;
        }
        else if (key == 'B')
        {
            menu_goto(MENU_MAIN, now);
            return;
        }
        menu_show();
        break;

    /* ---------- 系统设置（3 项） ---------- */
    case MENU_SUB_SYS:
        if (key == 'A')
        {
            menu_sel = (uint8_t)((menu_sel + 2U) % 3U);
        }
        else if (key == 'C')
        {
            menu_sel = (uint8_t)((menu_sel + 1U) % 3U);
        }
        else if (key == 'D')
        {
            if (menu_sel == 0U)
            {
                menu_goto(MENU_TIME, now);
            }
            else if (menu_sel == 1U)
            {
                log_offset = 0U;
                menu_mode = MENU_LOG;
                menu_transient = false;
                menu_status = BOARD_MENU_STATUS_NONE;
                menu_deadline = now + MENU_TIMEOUT_SECONDS;
                if (xQueueSend(log_request_queue, &log_offset, 0U) != pdTRUE)
                {
                    board_diagnostic_report(BOARD_DIAG_LOG_QUEUE_FULL);
                }
            }
            else
            {
                menu_sel = (uint8_t)security_config.second_factor_mode;
                menu_goto(MENU_COMBO, now);
            }
            return;
        }
        else if (key == 'B')
        {
            menu_goto(MENU_MAIN, now);
            return;
        }
        menu_show();
        break;

    /* ---------- 删除指纹：输入编号 ---------- */
    case MENU_FP_DEL:
        if ((key >= '0') && (key <= '9'))
        {
            if (menu_input_len < LOCK_INPUT_MAX_LENGTH)
            {
                menu_input[menu_input_len] = key;
                ++menu_input_len;
            }
        }
        else if (key == 'B')
        {
            if (menu_input_len > 0U)
            {
                --menu_input_len;
                menu_input[menu_input_len] = '\0';
            }
        }
        else if (key == '#')
        {
            uint32_t num = 0U;

            if (menu_input_len == 0U)
            {
                board_auth_feedback(false);
                menu_show();
                break;
            }
            for (uint8_t i = 0U; i < menu_input_len; ++i)
            {
                num = (num * 10U) + (uint32_t)(menu_input[i] - '0');
            }
            if ((num < 1U) || (num > (uint32_t)AS608_DURESS_PAGE))
            {
                board_auth_feedback(false);
                menu_input_len = 0U;
                menu_input[0] = '\0';
                menu_show();
                break;
            }
            if (AS608_DeleteChar((uint16_t)num, 1U) == 0)
            {
                queue_audit(LOCK_EVENT_DELETE, LOCK_METHOD_SYSTEM,
                            (uint16_t)num, 1U, now);
                board_auth_feedback(true);
                menu_result(MENU_SUB_FP, BOARD_MENU_STATUS_OK, now);
            }
            else
            {
                board_auth_feedback(false);
                menu_result(MENU_SUB_FP, BOARD_MENU_STATUS_NOENT, now);
            }
            return;
        }
        else if (key == 'C')
        {
            menu_goto(MENU_SUB_FP, now);
            return;
        }
        menu_show();
        break;

    /* ---------- 等刷卡 / 录指纹：只认 B 取消 ---------- */
    case MENU_FP_ENROLL:
    case MENU_CARD_WAIT:
        if (key == 'B')
        {
            menu_goto((menu_mode == MENU_FP_ENROLL) ? MENU_SUB_FP
                                                    : MENU_SUB_CARD,
                      now);
        }
        break;

    /* ---------- 开锁记录 ---------- */
    case MENU_LOG:
        menu_log_key(key, now);
        break;

    /* ---------- 开锁组合 ---------- */
    case MENU_COMBO:
        if (key == 'A')
        {
            menu_sel = (uint8_t)((menu_sel + 2U) % 3U);
        }
        else if (key == 'C')
        {
            menu_sel = (uint8_t)((menu_sel + 1U) % 3U);
        }
        else if (key == 'D')
        {
            security_config.second_factor_mode =
                (board_second_factor_mode_t)menu_sel;
            if (board_security_save(&security_config))
            {
                queue_audit(LOCK_EVENT_CONFIG_CHANGE, LOCK_METHOD_SYSTEM,
                            0U, 1U, now);
                board_auth_feedback(true);
                menu_result(MENU_SUB_SYS, BOARD_MENU_STATUS_SAVED, now);
            }
            else
            {
                board_auth_feedback(false);
                menu_result(MENU_SUB_SYS, BOARD_MENU_STATUS_FAIL, now);
            }
            return;
        }
        else if (key == 'B')
        {
            menu_goto(MENU_SUB_SYS, now);
            return;
        }
        menu_show();
        break;

    default:
        break;
    }
}

static void handle_board_input(const board_input_event_t *event, uint64_t now)
{
    uint32_t submitted_code = 0U;

    last_activity_at = now;

    switch (event->type)
    {
    // 处理密码输入
    case BOARD_AUTH_PIN:
        // 处理密码输入
        if (keypad_is_locked(now))
        {
            deny_access(LOCK_METHOD_PIN, event->user_id, true, now); // 键盘已锁定，拒绝访问
        }
        // 处理Duress密码输入
        else if (pin_credential_matches(
                     &security_config.duress_pin,
                     security_config.device_secret,
                     sizeof(security_config.device_secret),
                     event->digits, event->digit_count, true))
        {
            grant_duress(LOCK_METHOD_PIN, event->user_id, now); // 授权Duress密码
        }
        // 处理业主密码输入
        else if (pin_credential_matches(
                     &security_config.owner_pin,
                     security_config.device_secret,
                     sizeof(security_config.device_secret),
                     event->digits, event->digit_count, true))
        {
            keypad_clear_failures();
            if (security_config.second_factor_mode ==
                BOARD_SECOND_FACTOR_DISABLED)
            {
                grant_access(LOCK_METHOD_PIN, event->user_id, now); // 授权业主密码
            }
            // 处理第二因子密码输入
            else if ((pending_first_factor_until >= now) &&
                     (((security_config.second_factor_mode ==
                        BOARD_SECOND_FACTOR_FINGERPRINT_PIN) &&
                       (pending_first_factor == LOCK_METHOD_FINGERPRINT)) ||
                      ((security_config.second_factor_mode ==
                        BOARD_SECOND_FACTOR_RFID_PIN) &&
                       (pending_first_factor == LOCK_METHOD_RFID))))
            {
                const uint16_t user_id = pending_first_factor_user;
                clear_pending_factor();
                grant_access(LOCK_METHOD_PIN, user_id, now);
            }
            // **双因子开启，但没有合法 pending 第一因子**：拒绝开锁，UI 提示 `Use first factor first`。
            else
            {
                clear_pending_factor();
                deny_access(LOCK_METHOD_PIN, event->user_id, false, now);
                queue_ui(controller.state, "Use first factor first", true);
            }
        }
        // 临时访客一次性密码
        else if (visitor_credential_verify_and_consume( // 验证并消耗临时访客密码
                     &security_config.visitor,
                     security_config.device_secret,
                     sizeof(security_config.device_secret),
                     event->digits, event->digit_count, now))
        {

            keypad_clear_failures();
            if (board_security_save(&security_config))
            {
                grant_access(LOCK_METHOD_VISITOR, event->user_id, now);
            }
            else
            {
                board_diagnostic_report(BOARD_DIAG_SECURITY_STORAGE_FAILURE); // 存储失败
                deny_access(LOCK_METHOD_VISITOR, event->user_id, false, now);
            }
        }
        // 处理其他情况
        else
        {
            deny_access(LOCK_METHOD_PIN, event->user_id,
                        keypad_register_failure(now), now); // 拒绝访问
        }
        break;
        // 处理 TOTP 输入
    case BOARD_AUTH_TOTP:
        if (keypad_is_locked(now))
        {
            deny_access(LOCK_METHOD_TOTP, event->user_id, true, now);
            break;
        }
        // 验证 TOTP 密码
        for (uint8_t i = 0U; i < event->digit_count; ++i)
        {
            if ((event->digits[i] < '0') || (event->digits[i] > '9'))
            {
                deny_access(LOCK_METHOD_TOTP, event->user_id,
                            keypad_register_failure(now), now);
                return;
            }
            submitted_code = (submitted_code * 10U) +
                             (uint32_t)(event->digits[i] - '0');
        }
        if ((event->digit_count == 6U) &&
            totp_verify(security_config.totp_secret,
                        security_config.totp_secret_length,
                        now, 30U, 6U, submitted_code, 1U))
        {
            keypad_clear_failures();
            grant_access(LOCK_METHOD_TOTP, event->user_id, now);
        }
        else
        {
            deny_access(LOCK_METHOD_TOTP, event->user_id,
                        keypad_register_failure(now), now);
        }
        break;
        // 处理访客密码输入
    case BOARD_AUTH_VISITOR:
        if (keypad_is_locked(now))
        {
            deny_access(LOCK_METHOD_VISITOR, event->user_id, true, now);
            break;
        }
        if (visitor_credential_verify_and_consume(
                &security_config.visitor,
                security_config.device_secret,
                sizeof(security_config.device_secret),
                event->digits,
                event->digit_count,
                now))
        {
            if (board_security_save(&security_config))
            {
                keypad_clear_failures();
                grant_access(LOCK_METHOD_VISITOR, event->user_id, now);
            }
            else
            {
                board_diagnostic_report(BOARD_DIAG_SECURITY_STORAGE_FAILURE);
                deny_access(LOCK_METHOD_VISITOR, event->user_id, false, now);
            }
        }
        else
        {
            deny_access(LOCK_METHOD_VISITOR, event->user_id,
                        keypad_register_failure(now), now);
        }
        break;
        // 处理指纹输入
    case BOARD_AUTH_FINGERPRINT:
        if (security_config.second_factor_mode ==
            BOARD_SECOND_FACTOR_FINGERPRINT_PIN)
        {
            pending_first_factor = LOCK_METHOD_FINGERPRINT;
            pending_first_factor_user = event->user_id;
            pending_first_factor_until = now + SECOND_FACTOR_TIMEOUT_SECONDS;
            queue_ui(controller.state, "Enter PIN", true);
        }
        else
        {
            grant_access(LOCK_METHOD_FINGERPRINT, event->user_id, now);
            /* 48 号页 = 管理员指纹：开锁后 30 s 内按 'D' 免验进菜单
             * （与参考工程 app_ui.c 的行为一致） */
            if (event->user_id == (uint16_t)AS608_USER_PAGE_MAX)
            {
                admin_grace_until = now + ADMIN_GRACE_SECONDS;
            }
        }
        break;
        // 处理胁迫指纹输入
    case BOARD_AUTH_DURESS_FINGERPRINT:
        grant_duress(LOCK_METHOD_FINGERPRINT, event->user_id, now);
        break;
        // 处理 RFID 输入
    case BOARD_AUTH_RFID:
        if (security_config.second_factor_mode ==
            BOARD_SECOND_FACTOR_RFID_PIN)
        {
            pending_first_factor = LOCK_METHOD_RFID;
            pending_first_factor_user = event->user_id;
            pending_first_factor_until = now + SECOND_FACTOR_TIMEOUT_SECONDS;
            queue_ui(controller.state, "Enter PIN", true);
        }
        else
        {
            grant_access(LOCK_METHOD_RFID, event->user_id, now);
            /* 管理卡（flags 位 0 = 1）开锁后同样给 30 s 免验窗口 */
            if (board_card_last_role() == BOARD_CARD_ROLE_ADMIN)
            {
                admin_grace_until = now + ADMIN_GRACE_SECONDS;
            }
        }
        break;
        // 定期自动变化的密码，用`periodic_credential_verify`校验
    case BOARD_AUTH_PERIODIC:
        if (keypad_is_locked(now))
        {
            deny_access(LOCK_METHOD_PERIODIC, event->user_id, true, now);
        }
        else if (periodic_credential_verify(
                     &security_config.periodic_pin,
                     security_config.device_secret,
                     sizeof(security_config.device_secret),
                     event->digits, event->digit_count, true, now))
        {
            keypad_clear_failures();
            grant_access(LOCK_METHOD_PERIODIC, event->user_id, now);
        }
        else
        {
            deny_access(LOCK_METHOD_PERIODIC, event->user_id,
                        keypad_register_failure(now), now);
        }
        break;
        // 处理门开事件
    case BOARD_DOOR_OPENED:
        (void)lock_controller_handle(&controller, LOCK_CONTROL_DOOR_OPENED, now);
        door_opened_at = now;
        door_ajar_reported = false;
        break;
        // 处理门关事件
    case BOARD_DOOR_CLOSED:
        (void)lock_controller_handle(&controller, LOCK_CONTROL_DOOR_CLOSED, now);
        door_opened_at = 0U;
        door_ajar_reported = false;
        set_physical_state(controller.state);
        queue_audit(LOCK_EVENT_LOCK, LOCK_METHOD_SYSTEM, 0U, 1U, now);
        break;
        // 处理Tamper事件
    case BOARD_TAMPER_TRIGGERED:
        (void)lock_controller_handle(&controller, LOCK_CONTROL_TAMPER, now);
        set_physical_state(controller.state);
        queue_ui(controller.state, "Tamper alarm", true);
        queue_audit(LOCK_EVENT_TAMPER, LOCK_METHOD_SYSTEM, 0U, 0U, now);
        break;
        // 处理触摸唤醒事件
    case BOARD_TOUCH_WAKE:
        queue_ui(controller.state, "System awake", true);
        break;
        // 处理键盘活动事件
    case BOARD_KEYPAD_ACTIVITY:
        /* 数字键的本地回显由板级层直接写 OLED（board_port.c 的
         * ui_render_keypad_entry），这里不再下发 UI 消息，否则会把回显
         * 覆盖掉。本 case 存在的意义只是让函数开头的 last_activity_at = now
         * 生效，避免用户输入到一半被"静止 60 s 熄屏"逻辑关屏。 */
        break;
        // 处理显示最近日志事件
    case BOARD_SHOW_RECENT_LOG:
        /* ★ 2026-09-14 起不再响应：待机按 A/B 直接翻日志的旧行为取消
         * （用户反馈"A 键还会蹭出开锁信息"）。开锁记录统一从管理菜单
         * → 系统设置 → 开锁记录 进入。事件仍由 keypad_input 产出
         * （主机测试覆盖其语义），应用层忽略即可。 */
        break;
        // 处理菜单请求事件
    case BOARD_MENU_REQUEST:
        /*
         * 键盘 'D'：进入管理菜单（原"改主密码向导"已并入菜单）。
         *
         * 锁定期间直接拒绝：否则用户靠连按 'D' 就能反复提交主密码，
         * 把 60 s 锁定绕过去。管理员指纹(48 页)/管理卡开锁后的 30 s 内
         * 免验直达主菜单（与参考工程一致）。
         */
        if (menu_active)
        {
            break; /* 菜单模式下 'D' 走 BOARD_MENU_KEY，这里只是兜底 */
        }
        if (keypad_is_locked(now))
        {
            board_auth_feedback(false);
            queue_ui(controller.state, "Try again later", true);
        }
        else if (now < admin_grace_until)
        {
            menu_enter_main(now);
        }
        else
        {
            menu_begin_auth(now);
        }
        break;
        // 处理菜单按键事件
    case BOARD_MENU_KEY:
        /* 菜单模式下的原始按键（A/C/D/B/数字/#），由菜单状态机解释 */
        if (menu_active)
        {
            menu_key(event->digits[0], now);
        }
        break;
        // 处理键盘取消事件
    case BOARD_KEYPAD_CANCEL:
        /* 'C'：非菜单状态下只表示清空输入（已在 keypad_input 里做掉），
         * 这里兜底把可能残留的菜单收掉。 */
        if (menu_active)
        {
            menu_exit();
        }
        break;
        // 处理无认证事件
    case BOARD_AUTH_NONE:
    default:
        break;
    }
}
// 键盘输入任务：轮询键盘输入、读头、菜单、唤醒键，发送到输入队列
static void input_task(void *argument)
{
    board_input_event_t event;// 键盘输入事件缓冲区
    (void)argument;
    for (;;)
    {
        // 10 ms 节拍：与 buzzer_process()/rgb_process() 的设计周期一致，也让键盘消抖（KEYPAD_DEBOUNCE_MS=25）有足够的采样拍数。
        //*轮询键盘输入、读头、菜单、唤醒键
        if (board_poll_input(&event))// 有新事件
        {
            if (xQueueSend(input_queue, &event, pdMS_TO_TICKS(20U)) != pdTRUE)
            {
                board_diagnostic_report(BOARD_DIAG_INPUT_QUEUE_FULL); // 输入队列已满，无法发送事件
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10U));
    }
}
// 锁业务逻辑：本地按键 / 刷卡、菜单、远程指令处理、锁状态机、审计入队、低功耗判断
static void access_task(void *argument)
{
    board_input_event_t event; // 键盘输入事件缓冲区
    remote_command_t command;  // 远程指令缓冲区
    TickType_t last_tick = xTaskGetTickCount();
    (void)argument;

    for (;;)
    {
        if (xQueueReceive(input_queue, &event, pdMS_TO_TICKS(100U)) == pdTRUE) // 阻塞最多 100ms
        {
            // 处理键盘输入
            handle_board_input(&event, board_unix_time());
        }
        // 菜单状态机轮询，处理菜单内超时、UI 状态。
        menu_poll(board_unix_time());
        // 用 0 超时把队列里所有云端命令一次清空，避免积压。
        while (xQueueReceive(remote_queue, &command, 0U) == pdTRUE)
        {
            const uint64_t now = board_unix_time();
            last_activity_at = now;
            // 解析这条远程指令，执行对应动作（远程开锁、远程查询状态、远程添加 / 删除卡片指纹等）。
            handle_remote_command(&command, now);
            memset(&command, 0, sizeof(command));
        }

        if ((xTaskGetTickCount() - last_tick) >= pdMS_TO_TICKS(1000U))
        {
            const lock_state_t previous = controller.state; // 记录当前锁状态，用于判断是否改变状态
            const uint64_t now = board_unix_time();
            last_tick = xTaskGetTickCount(); // 更新上次轮询时间戳
            (void)lock_controller_handle(&controller, LOCK_CONTROL_TICK, now);
            // 处理待验证因子超时
            if ((pending_first_factor_until != 0U) &&
                (now > pending_first_factor_until))
            {
                clear_pending_factor();
                queue_ui(controller.state, "Second factor expired", true); // 提示待验证因子超时
            }
            // 锁状态改变时，更新物理状态和 UI 状态
            if (controller.state != previous)
            {
                set_physical_state(controller.state);
                queue_ui(controller.state, "Locked", true);
                queue_audit(LOCK_EVENT_LOCK, LOCK_METHOD_SYSTEM, 0U, 1U, now); // 记录锁状态改变事件
            }
            // 门未关闭且未报告门打开时，记录门打开事件
            if ((!controller.door_closed) && (!door_ajar_reported) &&
                (door_opened_at != 0U) &&
                (now >= (door_opened_at + DOOR_AJAR_DELAY_SECONDS)))
            {
                door_ajar_reported = true;
                board_door_ajar_warning();
                queue_ui(controller.state, "Door is still open", true); // 提示门未关闭
                queue_audit(LOCK_EVENT_DOOR_AJAR, LOCK_METHOD_SYSTEM,
                            0U, 0U, now); // 记录门打开事件
            }
            // 锁状态为锁定且门已关闭且无待验证因子超时，进入停止模式
            if ((controller.state == LOCK_STATE_LOCKED) &&
                controller.door_closed &&
                (pending_first_factor_until == 0U) &&
                /* 管理菜单期间绝不进 STOP：一睡下去键盘扫描就停了，
                 * 菜单会当场"死屏"。 */
                (!menu_active) &&
                (now >= (last_activity_at + STOP_MODE_DELAY_SECONDS)))
            {
                queue_ui(controller.state, NULL, false);
                if (board_try_stop_mode())
                {
                    last_activity_at = board_unix_time();
                    queue_ui(controller.state, "System awake", true);
                }
                else
                {
                    last_activity_at = now;
                }
            }
            board_watchdog_refresh();
        }
    }
}

static void handle_remote_command(remote_command_t *command, uint64_t now) // 处理远程命令
{
    // 验证远程指令
    const remote_command_result_t result = remote_command_verify(
        command,
        security_config.device_secret,
        sizeof(security_config.device_secret),
        now,
        5U,
        &security_config.last_remote_request_id);
    // 如果指令验证失败，记录认证失败事件
    if (result != REMOTE_COMMAND_ACCEPTED)
    {
        queue_audit(LOCK_EVENT_AUTH_FAILURE, LOCK_METHOD_REMOTE, 0U, 0U, now); // 记录认证失败事件
        return;
    }
    // 处理指令
    // 下发临时访客密码 / 凭证
    if (command->action == REMOTE_ACTION_ISSUE_VISITOR)
    {
        // 生成并保存访客凭证
        if ((!visitor_credential_issue(
                &security_config.visitor,
                security_config.device_secret,
                sizeof(security_config.device_secret),
                command->visitor_code,
                command->visitor_code_length,
                command->issued_at,
                command->expires_at,
                command->visitor_uses)) ||
            (!board_security_save(&security_config))) // 把 `security_config`写入 Flash。
        {
            queue_audit(LOCK_EVENT_NETWORK, LOCK_METHOD_REMOTE, 0U, 0U, now);// 记录网络事件
            return;
        }
    }
    // 其余所有远程指令（开锁、上锁、清除锁定）
    else if (!board_security_save(&security_config))
    {
        /* 重放状态必须先持久化落盘，之后才允许驱动执行机构。 */
        queue_audit(LOCK_EVENT_NETWORK, LOCK_METHOD_REMOTE, 0U, 0U, now); // 记录网络事件
        return;
    }

    switch (command->action)// 处理指令
    {
        // 开锁
    case REMOTE_ACTION_UNLOCK:
        grant_access(LOCK_METHOD_REMOTE, 0U, now);
        break;
        // 上锁
    case REMOTE_ACTION_LOCK:
        (void)lock_controller_handle(&controller, LOCK_CONTROL_FORCE_LOCK, now);
        set_physical_state(controller.state);
        queue_audit(LOCK_EVENT_LOCK, LOCK_METHOD_REMOTE, 0U, 1U, now);
        break;
        // 清除锁定
    case REMOTE_ACTION_CLEAR_LOCKOUT:
        memset(&pin_auth_state, 0, sizeof(pin_auth_state));
        (void)lock_controller_handle(&controller,
                                     LOCK_CONTROL_CLEAR_LOCKOUT, now);
        set_physical_state(controller.state);
        queue_audit(LOCK_EVENT_NETWORK, LOCK_METHOD_REMOTE, 0U, 1U, now);
        break;
        // 下发临时访客密码 / 凭证
    case REMOTE_ACTION_ISSUE_VISITOR:
        queue_audit(LOCK_EVENT_NETWORK, LOCK_METHOD_REMOTE, 0U, 1U, now);
        break;
    default:
        break;
    }
}
// 网络收发：接收云指令入队；消费通知队列，上传事件到云；50ms 轮询一次
static void network_task(void *argument) // 网络任务
{
    remote_command_t command;          // 远程指令缓冲区
    board_notification_t notification; // 通知缓冲区
    (void)argument;

    for (;;)
    {
        // 接收云端下发远程指令
        if (board_network_poll_command(&command)) // 接收云端下发远程指令
        {
            if (xQueueSend(remote_queue, &command,
                           pdMS_TO_TICKS(20U)) != pdTRUE) // 远程指令队列已满
            {
                // 远程指令队列已满，记录诊断事件
                board_diagnostic_report(BOARD_DIAG_REMOTE_QUEUE_FULL);
            }
            memset(&command, 0, sizeof(command));
        }
        // 读取通知队列，上报消息到云端
        if (xQueueReceive(notify_queue, &notification, 0U) == pdTRUE) // 从通知队列读取通知
        {
            uint32_t retry_delay_ms = 250U;
            // 循环尝试发布到云端，最多尝试8次
            while (!board_network_publish(&notification)) // 发布通知到云端
            {
                board_diagnostic_report(BOARD_DIAG_NETWORK_PUBLISH_FAILURE); // 上报诊断 `BOARD_DIAG_NETWORK_PUBLISH_FAILURE`
                vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
                if (retry_delay_ms < 8000U)
                {
                    retry_delay_ms *= 2U;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50U));
    }
}
// 日志任务：从日志队列读取事件，写入存储；50ms 轮询一次
static void log_task(void *argument) // 日志任务
{
    event_log_record_t record; // 日志记录缓冲区
    uint32_t offset;           // 日志偏移量缓冲区
    (void)argument;
    for (;;)
    {
        // 从日志队列读取事件，写入存储；50ms 轮询一次
        if (xQueueReceive(log_queue, &record, pdMS_TO_TICKS(50U)) == pdTRUE) // 从日志队列读取事件
        {
            // 写入日志存储
            if (!event_log_append(&event_log, &record))
            {
                board_diagnostic_report(BOARD_DIAG_LOG_STORAGE_FAILURE); // 上报诊断 `BOARD_DIAG_LOG_STORAGE_FAILURE`
            }
        }
        // 从日志请求队列读取偏移量，读取日志记录
        while (xQueueReceive(log_request_queue, &offset, 0U) == pdTRUE)
        {
            if (event_log_read_recent(&event_log, offset, &record)) // 读取日志记录
            {
                queue_ui_log(&record, true); // 发送日志记录到 UI 任务队列
            }
            else
            {
                queue_ui_log(NULL, false);
            }
        }
    }
}
// UI 任务，专门负责屏幕显示控制，**阻塞等待 UI 消息队列，有消息才渲染，无消息永久休眠**
static void ui_task(void *argument) // UI 任务
{
    app_ui_message_t ui; // UI 消息缓冲区
    (void)argument;
    for (;;)
    {
        // 永久阻塞等待消息。没有 UI 消息时，任务直接休眠，不占用 CPU
        if (xQueueReceive(ui_queue, &ui, portMAX_DELAY) == pdTRUE)
        {
            // 打开屏幕电源。
            board_ui_power(ui.power_on);
            // 当前是管理菜单页面
            if (ui.is_menu)
            {
                /* 菜单页：日志页有专门的渲染（带翻页提示），其余走通用渲染 */
                if (ui.menu_page == BOARD_MENU_LOG)
                {
                    board_menu_show_log(ui.has_log_record ? &ui.log_record
                                                          : NULL); // 显示日志记录
                }
                else
                {
                    board_menu_show(ui.menu_page, ui.menu_sel, ui.menu_aux,
                                    ui.message[0] != '\0' ? ui.message : NULL); // 录入指纹、添加卡片、删除卡片、系统设置等菜单页面
                }
            }
            // 不是菜单页面，屏幕点亮，并且携带一条日志记录
            else if (ui.power_on && ui.has_log_record)
            {
                board_ui_show_log(&ui.log_record); // 显示日志记录
            }
            // 非菜单页面，屏幕点亮，无日志记录
            else if (ui.power_on)
            {
                board_ui_show(ui.state, ui.message[0] != '\0' ? ui.message : NULL); // 显示状态和消息
            }
        }
    }
}

bool smart_lock_app_start(void)
{
    // 先准备日志存储后端
    const event_log_storage_t storage = {
        .context = NULL,
        .slot_count = EVENT_LOG_SLOT_COUNT,
        .read = board_log_read,
        .write = board_log_write,
        .erase = board_log_erase};
    // 硬件初始化+读配置+校验配置+初始化日志,任一失败即返回 false
    if ((!board_init()) || (!board_security_load(&security_config)) ||
        (!security_config_valid(&security_config)) ||
        (!event_log_init(&event_log, &storage)))
    {
        return false;
    }
    // 运行态清零:失败计数菜单/门状态/第二因子/锁控器
    memset(&pin_auth_state, 0, sizeof(pin_auth_state));
    menu_reset();
    door_opened_at = 0U;
    door_ajar_reported = false;
    clear_pending_factor();
    last_activity_at = board_unix_time();
    lock_controller_init(&controller, AUTO_LOCK_DELAY_SECONDS);
    set_physical_state(controller.state); // 上电闭锁状态
    // 创建队列
    input_queue = xQueueCreateStatic(INPUT_QUEUE_LENGTH,
                                     sizeof(board_input_event_t),
                                     input_queue_storage,
                                     &input_queue_control);// 输入事件队列
    remote_queue = xQueueCreateStatic(REMOTE_QUEUE_LENGTH,
                                      sizeof(remote_command_t),
                                      remote_queue_storage,
                                      &remote_queue_control);// 远程命令队列
    log_queue = xQueueCreateStatic(LOG_QUEUE_LENGTH,
                                   sizeof(event_log_record_t),
                                   log_queue_storage,
                                   &log_queue_control);// 日志队列
    notify_queue = xQueueCreateStatic(NOTIFY_QUEUE_LENGTH,
                                      sizeof(board_notification_t),
                                      notify_queue_storage,
                                      &notify_queue_control);// 通知队列
    ui_queue = xQueueCreateStatic(UI_QUEUE_LENGTH,
                                  sizeof(app_ui_message_t),
                                  ui_queue_storage,
                                  &ui_queue_control);// UI 消息队列
    log_request_queue = xQueueCreateStatic(LOG_REQUEST_QUEUE_LENGTH,
                                           sizeof(uint32_t),
                                           log_request_queue_storage,
                                           &log_request_queue_control);// 日志请求队列
    if ((input_queue == NULL) || (remote_queue == NULL) ||
        (log_queue == NULL) || (notify_queue == NULL) ||
        (ui_queue == NULL) || (log_request_queue == NULL))
    {
        return false;
    }
    // 创建任务
    if ((xTaskCreateStatic(input_task, "input", INPUT_TASK_STACK_WORDS, NULL, 3U,
                           input_task_stack, &input_task_control) == NULL) ||
        (xTaskCreateStatic(access_task, "access", ACCESS_TASK_STACK_WORDS, NULL, 4U,
                           access_task_stack, &access_task_control) == NULL) ||
        (xTaskCreateStatic(network_task, "network", NETWORK_TASK_STACK_WORDS, NULL, 2U,
                           network_task_stack, &network_task_control) == NULL) ||
        (xTaskCreateStatic(log_task, "log", LOG_TASK_STACK_WORDS, NULL, 1U,
                           log_task_stack, &log_task_control) == NULL) ||
        (xTaskCreateStatic(ui_task, "ui", UI_TASK_STACK_WORDS, NULL, 1U,
                           ui_task_stack, &ui_task_control) == NULL))
    {
        return false;
    }
    queue_ui(controller.state, "System ready", true);
    return true;
}
