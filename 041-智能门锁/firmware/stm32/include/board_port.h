#ifndef SMART_LOCK_BOARD_PORT_H
#define SMART_LOCK_BOARD_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "event_log.h"
#include "lock_controller.h"
#include "periodic_credential.h"
#include "pin_credential.h"
#include "remote_command.h"
#include "visitor_credential.h"

#define BOARD_DEVICE_SECRET_SIZE   32U
#define BOARD_TOTP_SECRET_MAX_SIZE 32U

typedef enum {
    BOARD_AUTH_NONE = 0,
    BOARD_AUTH_PIN,//密码认证
    BOARD_AUTH_TOTP,//TOTP认证
    BOARD_AUTH_VISITOR,//访客认证
    BOARD_AUTH_FINGERPRINT, //指纹认证
    BOARD_AUTH_DURESS_FINGERPRINT,// //指纹认证（高风险）
    BOARD_AUTH_RFID,//RFID认证
    BOARD_AUTH_PERIODIC,//周期认证
    BOARD_DOOR_OPENED,//门打开
    BOARD_DOOR_CLOSED,//门关闭
    BOARD_TAMPER_TRIGGERED,//篡改触发
    BOARD_TOUCH_WAKE,//触摸唤醒
    /*
     * 键盘上有按键动作，但还没构成一次认证请求（数字键、'*'、'C'）。
     * 板级层已经把这几位回显到 OLED 上，这个事件只是让应用层刷新
     * last_activity_at，免得输入到一半屏幕被"静止 60 s 熄屏"逻辑关掉。
     */
    BOARD_KEYPAD_ACTIVITY,
    BOARD_SHOW_RECENT_LOG,
    /*
     * 键盘 'D'：请求进入管理菜单。板级层只负责把按键翻成事件；
     * 是否真的进菜单、以及菜单的每一步怎么走，都由 smart_lock_app.c 决定。
     */
    BOARD_MENU_REQUEST,
    /*
     * 键盘 'C'：用户主动取消 / 清空当前输入。
     *
     * 为什么需要这个事件：'C' 以前只清输入缓冲、不产出任何事件，应用层
     * 根本看不见它。管理菜单要做到"按 C 退出"，就必须能收到这个信号。
     */
    BOARD_KEYPAD_CANCEL,
    /*
     * 管理菜单模式下的原始按键（2026-09-14 新增，见 board_menu_set_active）。
     *
     * 菜单激活期间，板级层不再把按键喂给 keypad_input（否则 '#' 会变成
     * BOARD_AUTH_PIN 把菜单步骤当成登录去开锁），而是把**每一个按键字符**
     * 原样放进 event->digits[0]（digit_count=1）发上来，由应用层菜单状态机
     * 自己解释：A/C 移动、D 进入、B 返回、'#' 确认、数字输入。
     */
    BOARD_MENU_KEY
} board_input_type_t;

/*
 * OLED 第 4 行（提示行）的文案。
 *
 * 这一行由板级层独占：输入时那几个 '*' 的回显就是它画的，所以向导的分步
 * 提示也必须由它来画 —— 否则用户一按键，提示就被"输入 ***"覆盖掉了。
 *
 * 文案统一用中文常量放在 board_port.c：整张中文字库是**按 board_port.c 的
 * 字符串字面量扫出来的**（tools/gen_oled_font.py）。汉字一旦散到别的文件
 * 就会漏字，在屏上显示成空心方框。因此其余模块只传这个枚举，不传字符串。
 *
 * 其余模块的状态消息仍沿用 ASCII（与 "Access granted" 等保持一致），
 * 这样新增字库压力最小。
 */
typedef enum {
    BOARD_PROMPT_DEFAULT = 0, /* 待机：请输入密码或指纹   */
    BOARD_PROMPT_OLD_PIN,     /* 改主密码：输入原密码     */
    BOARD_PROMPT_NEW_PIN,     /* 改主密码：输入新密码     */
    BOARD_PROMPT_CONFIRM_PIN  /* 改主密码：再次输入新密码 */
} board_prompt_t;

/*
 * 管理菜单的页面（2026-09-14 新增，键盘 'D' 进入）。
 *
 * 与 board_prompt_t 同一个设计约束：**所有中文文案都放在 board_port.c**，
 * 应用层只传这个枚举 + 序号，否则 gen_oled_font.py 扫不到用字，
 * 屏上就是一片空心方框。动态内容（输入回显、UID 十六进制）走 ASCII，
 * 由参数 line 带上来。
 *
 * 页面分两类：
 *   菜单类（MAIN/PIN/FP/CARD/DURESS/SYS/COMBO）：sel = 当前高亮项，
 *     按键 A=上移 C=下移 D=确认 B=返回（照搬参考工程 app_ui.c 的习惯）；
 *   功能/输入类（其余）：sel/aux 表达"当前是哪个子功能/第几步"，
 *     line 是 ASCII 动态行（输入回显等）。
 */
typedef enum {
    BOARD_MENU_MAIN = 0,  /* 功能菜单（5 项）                     */
    BOARD_MENU_PIN,       /* 密码管理                             */
    BOARD_MENU_FP,        /* 指纹管理                             */
    BOARD_MENU_CARD,      /* 卡片管理                             */
    BOARD_MENU_DURESS,    /* 胁迫管理                             */
    BOARD_MENU_SYS,       /* 系统设置                             */
    BOARD_MENU_COMBO,     /* 开锁组合（双因子开关）               */
    BOARD_MENU_INPUT,     /* 通用输入页（aux 选标题/提示语）      */
    BOARD_MENU_TIME,      /* 修改时间（YYMMDDHHMMSS）             */
    BOARD_MENU_FP_DEL,    /* 删除指纹（输编号 1~49）              */
    BOARD_MENU_FP_ENROLL, /* 指纹录入（sel=目标 aux=步骤）        */
    BOARD_MENU_CARD_WAIT, /* 等待刷卡（sel=模式 aux=状态）        */
    BOARD_MENU_LOG        /* 开锁记录（配合 has_log_record 路径） */
} board_menu_page_t;

/*
 * board_menu_input 页面的 aux 取值：决定标题与提示语（都在 board_port.c）。
 */
#define BOARD_MENU_INPUT_AUTH           0U /* 验证主密码（进菜单的门禁）  */
#define BOARD_MENU_INPUT_NEW_PIN        1U /* 修改主密码：输新密码        */
#define BOARD_MENU_INPUT_CONFIRM        2U /* 修改主密码：再输一遍        */
#define BOARD_MENU_INPUT_DURESS         3U /* 设置胁迫密码                */
#define BOARD_MENU_INPUT_FP_NUM         4U /* 录入指纹：输编号(1~47)      */
#define BOARD_MENU_INPUT_OLD_PIN        5U /* 修改主密码：输原密码        */
#define BOARD_MENU_INPUT_DURESS_CONFIRM 6U /* 设置胁迫密码：再输一遍      */

/*
 * 列表类页面底部（第 6 行）的操作结果提示（menu_draw_list 的 status）。
 * NONE 时显示常规提示行；其余短暂展示操作结果。
 */
#define BOARD_MENU_STATUS_NONE  0U
#define BOARD_MENU_STATUS_OK    1U /* 操作成功      */
#define BOARD_MENU_STATUS_FAIL  2U /* 操作失败      */
#define BOARD_MENU_STATUS_DUP   3U /* 已存在/重复   */
#define BOARD_MENU_STATUS_FULL  4U /* 容量已满      */
#define BOARD_MENU_STATUS_NOENT 5U /* 没有这一条    */
#define BOARD_MENU_STATUS_SAVED 6U /* 已保存        */

typedef enum {
    BOARD_DIAG_INPUT_QUEUE_FULL = 1,
    BOARD_DIAG_REMOTE_QUEUE_FULL,
    BOARD_DIAG_LOG_QUEUE_FULL,
    BOARD_DIAG_NOTIFY_QUEUE_FULL,
    BOARD_DIAG_UI_QUEUE_FULL,
    BOARD_DIAG_LOG_STORAGE_FAILURE,
    BOARD_DIAG_SECURITY_STORAGE_FAILURE,
    BOARD_DIAG_NETWORK_PUBLISH_FAILURE
} board_diagnostic_code_t;

typedef enum {
    BOARD_SECOND_FACTOR_DISABLED = 0,
    BOARD_SECOND_FACTOR_FINGERPRINT_PIN,
    BOARD_SECOND_FACTOR_RFID_PIN
} board_second_factor_mode_t;

typedef struct {
    board_input_type_t type;            // 输入类型
    uint16_t user_id;                   // 用户 ID
    char digits[LOCK_INPUT_MAX_LENGTH]; // 输入的数字
    uint8_t digit_count;                // 输入的数字数量
} board_input_event_t;
// 安全配置结构体
typedef struct {
    uint8_t device_secret[BOARD_DEVICE_SECRET_SIZE];
    uint8_t totp_secret[BOARD_TOTP_SECRET_MAX_SIZE];
    uint8_t totp_secret_length;
    pin_credential_t owner_pin;
    pin_credential_t duress_pin;
    periodic_credential_t periodic_pin;
    visitor_credential_t visitor;
    uint64_t last_remote_request_id;
    board_second_factor_mode_t second_factor_mode;
} board_security_config_t;

typedef struct {
    lock_state_t state;
    lock_event_type_t event_type;
    lock_auth_method_t auth_method;
    uint16_t user_id;
    uint64_t unix_time;
    uint8_t result;
} board_notification_t;

bool board_init(void);
uint64_t board_unix_time(void);
void board_watchdog_refresh(void);
/* 进入 STOP 低功耗模式，唤醒后恢复时钟与 DMA 再返回。 */
bool board_try_stop_mode(void);
void board_diagnostic_report(board_diagnostic_code_t code);

/*
 * PA8 唤醒键的 EXTI8 服务函数调用它。
 *
 * 中断里只置一个 volatile 标志，真正的"唤醒"动作（点亮屏幕、刷新待机界面）
 * 由 board_poll_input() 在 input 任务上下文里翻译成 BOARD_TOUCH_WAKE 事件，
 * 这样 ISR 不碰任何 FreeRTOS 对象，也就不需要临界区。
 */
void board_wake_irq_notify(void);

/*
 * 非阻塞输入轮询。指纹 / RFID 事件表示板级驱动已经
 * 完成了一次针对已启用且已登记用户的成功匹配。
 */
bool board_poll_input(board_input_event_t *event);

/* 非阻塞网络轮询。ESP8266 传输层把 JSON 解析成该类型。 */
bool board_network_poll_command(remote_command_t *command);
bool board_network_publish(const board_notification_t *notification);

/* 加载 / 保存必须使用带版本号、CRC 校验、掉电安全的信封结构。 */
bool board_security_load(board_security_config_t *config);
bool board_security_save(const board_security_config_t *config);

void board_lock_set(bool locked);
void board_alarm_set(bool active);
void board_auth_feedback(bool success);
void board_door_ajar_warning(void);
void board_ui_power(bool enabled);
void board_ui_show(lock_state_t state, const char *message);
void board_ui_show_log(const event_log_record_t *record);

/*
 * 切换 OLED 第 4 行的提示语（改密码向导用）。
 *
 * 只置标志、不在这里碰 I2C：真正的重画交给 input 任务（board_poll_input），
 * 让"第 4 行"只有 input 任务与 ui 任务两个写者，与改动前保持一致，
 * 避免再多出第三个任务去写软件 I2C。
 */
void board_ui_set_prompt(board_prompt_t prompt);

/* ------------------------------------------------------------------ */
/* 管理菜单（2026-09-14 新增）                                          */
/* ------------------------------------------------------------------ */

/*
 * 进入 / 退出菜单模式。激活后 board_poll_input() 的行为改变：
 *   1. 按键不再喂 keypad_input（不会拼出 BOARD_AUTH_PIN），而是逐键发
 *      BOARD_MENU_KEY，由应用层菜单状态机解释；
 *   2. 不再轮询指纹（AS608）与刷卡（RC522）—— 录入指纹/添卡删卡期间，
 *      模块归菜单流程独占，后台识别必须让路（同参考工程 reader_exclusive）；
 *   3. 键盘无操作超时（keypad_input_tick）也一并停掉。
 * 退出时恢复以上全部行为，并清空 PIN 输入缓冲与向导提示行。
 */
void board_menu_set_active(bool active);

/*
 * 渲染一页菜单/功能页（由 ui 任务在收到 is_menu 消息时调用）。
 *   page  页面（决定标题、菜单项、提示行文案）
 *   sel   当前高亮项 / 子参数（录入目标、刷卡模式等）
 *   aux   次级参数（输入页标题、录入步骤、刷卡结果等）
 *   line  ASCII 动态行（输入回显 "1234"、UID "1A2B3C4D" 等），可为 NULL
 */
void board_menu_show(board_menu_page_t page, uint8_t sel, uint8_t aux,
                     const char *line);

/*
 * 渲染"开锁记录"页：内容与 board_ui_show_log() 相同，
 * 但第 6 行换成菜单翻页提示（A 上翻 / C 下翻 / B 退出）。
 */
void board_menu_show_log(const event_log_record_t *record);

/* 卡片角色（存进卡片表条目的 flags 字节，不改表布局） */
#define BOARD_CARD_ROLE_NORMAL 0U
#define BOARD_CARD_ROLE_ADMIN  1U

/*
 * 非阻塞读一次 RC522。菜单模式下由应用层菜单状态机周期调用
 * （此时 board_poll_input 已停掉自己的读卡轮询）。读到**新**卡返回 true
 * 并填 uid；同一张卡按住不放会在去重窗口内被压掉。
 */
bool board_card_read(uint8_t *uid);

/* 把一张卡写入卡片表并落盘（role 见 BOARD_CARD_ROLE_*）。 */
bool board_card_add(const uint8_t *uid, uint8_t role, uint16_t user_id);

/* 从卡片表删除一张卡并落盘；卡不在表里返回 false。 */
bool board_card_remove(const uint8_t *uid);

/* 当前卡片表条数（显示用）。 */
uint8_t board_card_count(void);

/* 最近一次 poll_card()/board_card_read() 读到的卡的登记角色（0 普通 1 管理员） */
uint8_t board_card_last_role(void);

/* W25Q64 最小可行映射：每条日志占用一个 4 KiB 擦除扇区。 */
bool board_log_read(void *context, uint32_t slot, event_log_record_t *record);
bool board_log_write(void *context, uint32_t slot,
                     const event_log_record_t *record);
bool board_log_erase(void *context, uint32_t slot);

/*
 * 应用层状态查询：当前键盘是否处于"连续错误锁定"状态。
 *
 * 由 smart_lock_app.c 实现（它持有 pin_auth_state）。之所以做成回调而不是
 * 让云端模块直接读，是因为云端模块运行在 network 任务，而锁定状态由
 * access 任务维护，必须经由应用层给出的只读入口访问。
 */
bool board_app_is_locked_out(void);

#endif
