/*
 * 板级适配层：把可移植的应用核心（firmware/app）接到真实硬件上。
 *
 * 实现 board_port.h 声明的全部接口，方法上尽量复用参考工程
 * C:\Users\71563\Desktop\智能门锁\STM32_SmartLock 里已经上板验证过的驱动：
 *
 *   SYSTEM/   sys.c delay.c usart.c        （时钟/NVIC/JTAG释放、DWT延时、三路串口）
 *   HARDWARE/ i2c_soft.c oled.c ds3231.c mfrc522.c w25q64.c
 *             servo.c buzzer.c rgb_led.c as608.c esp8266.c
 *   MIDDLEWARE/ cJSON.c MqttKit.c
 *   USER/     app_wifi.c                   （OneNET 物模型，**原样零改动**）
 *
 * 只有两处不复用：
 *   1) 键盘扫描 —— 引脚表已与参考工程 key_4x4.c **完全对齐**（行 PA11/PA12/
 *      PA15/PB3、列 PB4/PB5/PB6/PB7），键位表也与参考工程 s_key_map 一致；
 *      但扫描逻辑改走本工程的 keypad_input.c（I/O 可注入、可在主机上单测）。
 *   2) 物模型下行 —— app_wifi.c 的 9 个 app_* 钩子由本文件底部的"兼容垫片"
 *      提供，把下行请求翻译成本工程的 remote_command_t。
 */

#include "board_port.h"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

/* StdPeriph 与引脚表要最先引入：smart_lock_pinmap.h 依赖 GPIO/EXTI 的宏 */
#include "stm32f10x.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_iwdg.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_rcc.h"
#include "misc.h"

#include "smart_lock_board_config.h"
#include "smart_lock_pinmap.h"

/* 本工程自带的可移植模块 */
#include "config_store.h"
#include "keypad_input.h"
#include "pin_credential.h"
#include "remote_command.h"
#include "visitor_credential.h"

/* 参考工程驱动 */
#include "as608.h"
#include "buzzer.h"
#include "delay.h"
#include "ds3231.h"
#include "esp8266.h"
#include "i2c_soft.h"
#include "mfrc522.h"
#include "oled.h"
#include "rgb_led.h"
#include "servo.h"
#include "sys.h"
#include "usart.h"
#include "w25q64.h"

/* app_wifi.c 的物模型逻辑原样复用，它的 9 个 app_* 钩子由本文件实现；
 * 这里包含参考工程的头文件，目的是让编译器用同一份原型做签名核对。 */
#include "app_alarm.h"
#include "app_auth.h"
#include "app_common.h"
#include "app_lock.h"
#include "app_log.h"
#include "app_ui.h"
#include "app_wifi.h"

/* ------------------------------------------------------------------ */
/* 轮询节拍                                                            */
/* ------------------------------------------------------------------ */
/* board_poll_input() 由 input_task 每 10 ms 调一次 */
#define POLL_PERIOD_MS            10U
#define CARD_POLL_TICKS           10U  /* 约 100 ms 扫一次卡（2026-09-15 由 300ms 提速；
                                        * 配合驱动把"无卡超时"从 25ms 缩到 ≈8ms，
                                        * 有效寻卡次数从 3.3 次/秒提到约 30 次/秒） */
#define FINGER_POLL_TICKS         40U  /* 约 400 ms 轮询一次指纹 */
#define CARD_DUPLICATE_MS       3000U  /* 同一张卡 3 s 内只上报一次 */

/* ------------------------------------------------------------------ */
/* 卡片 UID 表（W25Q64，0x002000 起的独立扇区）                        */
/* ------------------------------------------------------------------ */
#define CARD_TABLE_MAGIC      UINT32_C(0x44524143) /* 'CARD' */
#define CARD_TABLE_VERSION    UINT16_C(1)
#define CARD_TABLE_HEADER     UINT16_C(8)
#define CARD_TABLE_ENTRY_SIZE UINT16_C(8)  /* uid[4] + user_id + flags + 2 保留 */

typedef struct {
    uint8_t uid[FLASH_CARD_UID_SIZE];
    uint16_t user_id;
    uint8_t role;   /* BOARD_CARD_ROLE_*：存进条目 flags 字节，不改表布局 */
} card_entry_t;

typedef struct {
    uint8_t uid[FLASH_CARD_UID_SIZE];
    uint16_t user_id;
    uint32_t seen_at_ms;
    bool valid;
} last_card_t;

/* ------------------------------------------------------------------ */
/* 云端下行桥（app_wifi.c 的钩子写进这里，board_network_poll_command 取出） */
/* ------------------------------------------------------------------ */
/*
 * 两者都运行在 network 任务里（board_network_poll_command() 内部会调用
 * app_wifi_process()），因此这个环形缓冲是单任务访问，不需要加锁。
 * 上层若改成从别的任务调用 app_wifi_*，必须补临界区。
 */
#define REMOTE_REQUEST_RING       4U

/* ------------------------------------------------------------------ */
/* 模块静态状态                                                        */
/* ------------------------------------------------------------------ */
static keypad_matrix_io_t s_matrix_io;
static keypad_scan_t s_scan;// 键盘扫描状态机
static keypad_input_t s_keypad;

static card_entry_t s_card_table[FLASH_CARD_TABLE_CAPACITY];
static uint8_t s_card_count;
static last_card_t s_last_card;
/* 最近一次读到的卡的登记角色（0 普通 1 管理员），供应用层判断"管理卡" */
static uint8_t s_card_last_role;

/*
 * 管理菜单模式（board_menu_set_active() 切换）：
 *   true 期间 board_poll_input 不再喂 keypad_input、也不再轮询指纹/刷卡，
 *   按键逐个变成 BOARD_MENU_KEY 上抛，读头模块让给菜单流程独占。
 */
static bool s_menu_active;

static remote_command_t s_remote_ring[REMOTE_REQUEST_RING];
static uint8_t s_remote_head;
static uint8_t s_remote_tail;

/* board_security_load/save 时缓存下来的敏感量与重放序号，
 * 供兼容垫片在需要时本地签一条命令用（见文件末尾说明）。 */
static uint8_t s_device_secret[BOARD_DEVICE_SECRET_SIZE];
static uint64_t s_remote_request_id;

static bool s_lock_engaged = true;   /* 舵机当前是否在锁闭位 */
static bool s_alarm_active;

static uint32_t s_poll_ticks;

/* PA8 唤醒键：EXTI8 中断里置位，board_poll_input() 取用后清零 */
static volatile bool s_wake_pending;

/*
 * OLED 第 4 行（提示行）当前用哪条文案。默认 = 待机提示语。
 * 改密码向导通过 board_ui_set_prompt() 切换（见 board_port.h 的 board_prompt_t）。
 */
static board_prompt_t s_prompt;

/*
 * board_ui_set_prompt() 只置这个标志，真正的重画由 input 任务在
 * board_poll_input() 里做。这样"第 4 行"的写者仍然只有 input 任务和 ui 任务，
 * 不会再多出一个任务去写软件 I2C（那两条线是没有锁的）。
 */
static volatile bool s_prompt_dirty;

static void keypad_hardware_init(void);
static bool card_table_load(void);
static bool card_table_save(void);
static int8_t card_table_find_index(const uint8_t *uid);
static bool poll_card(board_input_event_t *event);
static bool poll_finger(board_input_event_t *event);
static void ui_render_status(lock_state_t state, const char *message);
static void ui_render_keypad_entry(void);
static const char *prompt_text(board_prompt_t prompt);

/* ================================================================== */
/* 初始化                                                              */
/* ================================================================== */

static void keypad_hardware_init(void)
{
    GPIO_InitTypeDef gpio;

    /* 引脚定义集中在 smart_lock_pinmap.h（已与参考工程 key_4x4.c 完全对齐）：
     *   行 PA11/PA12/PA15/PB3（推挽输出，扫描时逐行拉低）
     *   列 PB4/PB5/PB6/PB7（上拉输入，按下读到低）
     * PA15/PB3/PB4 原本是 JTAG，sys_init() 已经把它们释放成普通 GPIO。
     *
     * 对齐参考接线后，行与列**都跨越 GPIOA/GPIOB 两个端口**（行 3 根在 A、
     * 1 根在 B，列 4 根全在 B），所以不能再假设"行一个口、列一个口"，
     * 一律按端口分组把要配置的脚合并到一次 GPIO_Init 里。 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    gpio.GPIO_Speed = GPIO_Speed_50MHz;

    /* 行：推挽输出（ROW1~ROW3 在 GPIOA，ROW4 在 GPIOB） */
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Pin = KEYPAD_ROW1_PIN | KEYPAD_ROW2_PIN | KEYPAD_ROW3_PIN;
    GPIO_Init(KEYPAD_ROW1_PORT, &gpio);
    gpio.GPIO_Pin = KEYPAD_ROW4_PIN;
    GPIO_Init(KEYPAD_ROW4_PORT, &gpio);

    /* 列：上拉输入（按下读到低）。4 列按现接线全在 GPIOB。 */
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Pin = KEYPAD_COL1_PIN | KEYPAD_COL2_PIN |
                    KEYPAD_COL3_PIN | KEYPAD_COL4_PIN;
    GPIO_Init(GPIOB, &gpio);

    /* 全部行输出高电平（未选中） */
    GPIO_SetBits(KEYPAD_ROW1_PORT, KEYPAD_ROW1_PIN);
    GPIO_SetBits(KEYPAD_ROW2_PORT, KEYPAD_ROW2_PIN);
    GPIO_SetBits(KEYPAD_ROW3_PORT, KEYPAD_ROW3_PIN);
    GPIO_SetBits(KEYPAD_ROW4_PORT, KEYPAD_ROW4_PIN);
}

/* 键盘矩阵 I/O 注入实现：一次只把一行拉低，其余行抬高 */
static void keypad_select_row(void *context, uint8_t row)
{
    (void)context;

    /* 先把 4 行全部抬高（取消选中），再把目标行单独拉低。
     * 必须按"每行自己的 PORT"操作：对齐参考接线后行跨越 A/B 两口
     * （ROW1~ROW3=GPIOA，ROW4=GPIOB），写死端口就会去拉错端口上的同号脚。 */
    GPIO_SetBits(KEYPAD_ROW1_PORT, KEYPAD_ROW1_PIN);
    GPIO_SetBits(KEYPAD_ROW2_PORT, KEYPAD_ROW2_PIN);
    GPIO_SetBits(KEYPAD_ROW3_PORT, KEYPAD_ROW3_PIN);
    GPIO_SetBits(KEYPAD_ROW4_PORT, KEYPAD_ROW4_PIN);

    switch (row) {
    case 0U: GPIO_ResetBits(KEYPAD_ROW1_PORT, KEYPAD_ROW1_PIN); break;
    case 1U: GPIO_ResetBits(KEYPAD_ROW2_PORT, KEYPAD_ROW2_PIN); break;
    case 2U: GPIO_ResetBits(KEYPAD_ROW3_PORT, KEYPAD_ROW3_PIN); break;
    case 3U: GPIO_ResetBits(KEYPAD_ROW4_PORT, KEYPAD_ROW4_PIN); break;
    default: break; /* KEYPAD_ROWS = 取消全部选中，上面已经抬高 */
    }
}

static bool keypad_read_column(void *context, uint8_t column)
{
    (void)context;
    switch (column) {
    case 0U: return GPIO_ReadInputDataBit(KEYPAD_COL1_PORT, KEYPAD_COL1_PIN) == Bit_RESET;
    case 1U: return GPIO_ReadInputDataBit(KEYPAD_COL2_PORT, KEYPAD_COL2_PIN) == Bit_RESET;
    case 2U: return GPIO_ReadInputDataBit(KEYPAD_COL3_PORT, KEYPAD_COL3_PIN) == Bit_RESET;
    case 3U: return GPIO_ReadInputDataBit(KEYPAD_COL4_PORT, KEYPAD_COL4_PIN) == Bit_RESET;
    default: return false;
    }
}

static void wake_key_init(void)
{
    GPIO_InitTypeDef gpio;
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);

    gpio.GPIO_Pin = WAKE_KEY_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(WAKE_KEY_PORT, &gpio);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, WAKE_KEY_PIN_SOURCE);
    exti.EXTI_Line = WAKE_KEY_EXTI_LINE;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Falling; /* 按下（低有效）唤醒 */
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);

    /* 优先级必须落在 configMAX_SYSCALL_INTERRUPT_PRIORITY 之下，
     * 否则 ISR 里调用 FromISR 版 FreeRTOS API 会触发断言。 */
    nvic.NVIC_IRQChannel = WAKE_KEY_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 0x0FU;
    nvic.NVIC_IRQChannelSubPriority = 0U;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

/* EXTI9_5_IRQHandler() 调用：只置标志，不做别的事 */
void board_wake_irq_notify(void)
{
    s_wake_pending = true;
}

static void watchdog_init(void)
{
#if SMART_LOCK_ENABLE_IWDG
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_256);
    IWDG_SetReload((uint16_t)SMART_LOCK_IWDG_RELOAD);
    IWDG_ReloadCounter();
    IWDG_Enable();
#endif
}

bool board_init(void)
{
    /* 1) 中断分组 + AFIO 时钟 + 释放 PA15/PB3/PB4（否则键盘不可用） */
    sys_init();
    delay_init();

    /* 2) 三路串口：USART1 ESP8266 / USART2 指纹 / USART3 调试打印 */
    usart3_init(DEBUG_UART_BAUD);
    usart1_init(ESP8266_UART_BAUD);
    usart2_init(AS608_UART_BAUD);


    /* 3) 显示与本地交互 */
    IIC_Init();
    OLED_Init();
    /* SSD1306 上电后显存内容是不确定的，而界面只写 0/2/4/6 四行（第 3 页
     * 不属于任何一行，从来没人写），不先清屏就会在屏上留下一条随机噪点带、
     * 以及各短行右侧的残像 —— 这就是上板看到的"乱屏"。 */
    OLED_Clear();
    buzzer_init();
    rgb_init();

    /* 4) 存储：日志与配置都放 W25Q64 */
    if (W25Q64_Init() != 1U) {
        board_diagnostic_report(BOARD_DIAG_LOG_STORAGE_FAILURE);
        return false;
    }


    /* 5) 射频读卡：初始化 */
    {
        MFRC522_Init();

        /* 参考工程实测：MCU 复位后 RC522 经常**首次初始化不成功**（MISO 无驱动 /
         * Version 0x00）—— 上电瞬间芯片内部状态机没起来，写进去的配置读不回来。
         * 这时只要再跑一次 MFRC522_Init()（内含 PC14 RST 硬复位脉冲 + 软复位）
         * 就能把芯片救回来。缺了这一步，上板现象就是"卡贴上去没反应"。
         * 若二次仍不通，才需要查线（SCK/MOSI/MISO/供电）或 RST 硬复位通路。 */
        if (!MFRC522_Check()) {
            MFRC522_Init();
        }
        if (MFRC522_Check()) {
            printf("[OK] MFRC522 RFID (Version 0x%02X)\r\n",
                   (unsigned)MFRC522_Version());
        } else {
            printf("[ERR] MFRC522 no response (Version 0x%02X) -- check SCK/MOSI/MISO/PWR\r\n",
                   (unsigned)MFRC522_Version());
        }
    }

    /* 6) 时钟：优先真实 DS3231（芯片里存 UTC），探测不到会自动回落到软时钟。
     *    这里把结果和时间打出来，一眼就能看出模块到底在不在线。 */
    {
        const uint8_t rtc_ok = DS3231_Init();
        DS3231_Time now;

        DS3231_GetTime(&now);
        printf("[RTC] %s %02u-%02u-%02u %02u:%02u:%02u UTC\r\n",
               rtc_ok ? "DS3231 online" : "soft clock (no DS3231 ack)",
               (unsigned)now.year, (unsigned)now.month, (unsigned)now.date,
               (unsigned)now.hour, (unsigned)now.min, (unsigned)now.sec);
    }

    /* 7) 锁体执行器：上电先回到锁闭位 */
    servo_init();
    servo_lock();
    s_lock_engaged = true;

    /* 8) 键盘与唤醒键 */
    keypad_hardware_init();
    wake_key_init();

    s_matrix_io.context = NULL;
    s_matrix_io.select_row = keypad_select_row;
    s_matrix_io.read_column = keypad_read_column;
    keypad_scan_init(&s_scan);
    keypad_input_init(&s_keypad);

    (void)memset(&s_last_card, 0, sizeof(s_last_card));
    s_remote_head = 0U;
    s_remote_tail = 0U;
    s_poll_ticks = 0U;
    s_alarm_active = false;

    (void)card_table_load();

    /* 9) 云端业务：注册 MQTT 下行回调并打印三元组 */
    app_wifi_init();

    watchdog_init();

    printf("[BOOT] smart lock board ready\r\n");
    return true;
}

/* ================================================================== */
/* 时间与看门狗                                                        */
/* ================================================================== */

uint64_t board_unix_time(void)
{
#if DS3231_USE_SOFT
    return (uint64_t)DS3231_SoftNowUnix();
#else
    DS3231_Time now;
    DS3231_GetTime(&now);
    return (uint64_t)DS3231_GetUnix(&now);
#endif
}

void board_watchdog_refresh(void)
{
#if SMART_LOCK_ENABLE_IWDG
    IWDG_ReloadCounter();
#endif
}

bool board_try_stop_mode(void)
{
#if SMART_LOCK_ENABLE_STOP_MODE
    /* 进 STOP 前先确保锁体在锁闭位、屏幕已关、射频已静默 */
    board_ui_power(false);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);
    /* 唤醒后时钟回到 HSI，必须重新初始化到 72 MHz 并恢复外设 */
    SystemInit();
    return true;
#else
    return false;
#endif
}

/* ================================================================== */
/* 诊断输出                                                            */
/* ================================================================== */

static const char *diagnostic_name(board_diagnostic_code_t code)
{
    switch (code) {
    case BOARD_DIAG_INPUT_QUEUE_FULL:        return "input queue full";
    case BOARD_DIAG_REMOTE_QUEUE_FULL:       return "remote queue full";
    case BOARD_DIAG_LOG_QUEUE_FULL:          return "log queue full";
    case BOARD_DIAG_NOTIFY_QUEUE_FULL:       return "notify queue full";
    case BOARD_DIAG_UI_QUEUE_FULL:           return "ui queue full";
    case BOARD_DIAG_LOG_STORAGE_FAILURE:     return "log storage failure";
    case BOARD_DIAG_SECURITY_STORAGE_FAILURE:return "security storage failure";
    case BOARD_DIAG_NETWORK_PUBLISH_FAILURE: return "network publish failure";
    default:                                 return "unknown";
    }
}

void board_diagnostic_report(board_diagnostic_code_t code)
{
    printf("[DIAG] %s\r\n", diagnostic_name(code));
}

/* ================================================================== */
/* 输入：键盘 / 读卡 / 指纹                                            */
/* ================================================================== */

/* 按 UID 找条目下标；没找到返回 -1（调用方据此拿到 user_id 与 role） */
static int8_t card_table_find_index(const uint8_t *uid)
{
    for (uint8_t i = 0U; i < s_card_count; ++i) {
        if (memcmp(s_card_table[i].uid, uid, FLASH_CARD_UID_SIZE) == 0) {
            return (int8_t)i;
        }
    }
    return -1;
}

/* 卡片登记/删除：始终编译（管理菜单要用），与"首卡自学习"开关无关 */
static bool card_table_enroll(const uint8_t *uid, uint16_t user_id, uint8_t role)
{
    if (s_card_count >= (uint8_t)FLASH_CARD_TABLE_CAPACITY) {
        return false;
    }
    (void)memcpy(s_card_table[s_card_count].uid, uid, FLASH_CARD_UID_SIZE);
    s_card_table[s_card_count].user_id = user_id;
    s_card_table[s_card_count].role = role;
    ++s_card_count;
    if (!card_table_save()) {
        --s_card_count;
        return false;
    }
    printf("[CARD] enrolled user %u role %u\r\n",
           (unsigned)user_id, (unsigned)role);
    return true;
}

/* 从卡片表删掉一张卡（后面的条目前移补位），并落盘。 */
static bool card_table_remove_at(uint8_t index)
{
    if (index >= s_card_count) {
        return false;
    }
    for (uint8_t i = index; ((i + 1U) < s_card_count); ++i) {
        s_card_table[i] = s_card_table[i + 1U];
    }
    --s_card_count;
    (void)memset(&s_card_table[s_card_count], 0, sizeof(s_card_table[0]));
    if (!card_table_save()) {
        return false;
    }
    printf("[CARD] removed entry %u\r\n", (unsigned)index);
    return true;
}

/* ------------------------------------------------------------------ */
/* 读卡公共层（2026-09-15 读取可靠性优化③④）                            */
/* ------------------------------------------------------------------ */
/*
 * 为什么要有这一层：原来 poll_card()/board_card_read() 各自只做
 * "一次 Request + 一次 Anticoll"，一次失败就整轮作废。用户体感是
 * "时灵时不灵"（手上还在移动、卡刚进场的瞬间采样不到就错过一整轮）。
 *
 * 这里统一做三件事：
 *   ① 一次轮询内**重试多次**寻卡（默认 3 次）——把单轮命中率从约 60%
 *      拉到 95% 以上（每次尝试都是独立的 REQA/ATQA 往返）；
 *   ② 读到 UID 后**主动 HALT**，让卡回到 IDLE，可以立刻被再次寻到；
 *   ③ **自愈**：连续多轮一次都没寻到卡时做一次两级自愈（见 card_reader_heal）。
 */
#define CARD_READ_ATTEMPTS      3U   /* 单轮寻卡重试次数 */
#define CARD_HEAL_AFTER_POLLS  50U   /* 连续 50 轮无卡（≈5 s）后做一次自检 */
#define CARD_REINIT_AFTER_HEAL  6U   /* 连续无卡时每 6 次自愈（≈30 s）强制整片重初始化 */

static uint16_t s_card_heal_streak;  /* 连续"一无所获"的轮数 */
static uint8_t  s_card_heal_count;   /* 自愈次数：用于周期性强制整片重初始化 */

static bool read_card_uid(uint8_t *uid)
{
    uint8_t tag_type[2];
    uint8_t attempt;

    for (attempt = 0U; attempt < CARD_READ_ATTEMPTS; ++attempt) {
        if (MFRC522_Request(PICC_REQIDL, tag_type) != MI_OK) {
            continue;                       /* 本刻无卡/未回包，立刻再试 */
        }
        if (MFRC522_Anticoll(uid) != MI_OK) {
            continue;                       /* 防冲撞/BCC 校验失败：绝不放行 */
        }
        /* 读到就把卡放回 IDLE，卡继续压在读头上也能被下一轮寻到 */
        MFRC522_Halt();
        return true;
    }
    return false;
}

/* 连续多轮无卡时的两级自愈（2026-09-15 加固；修"读卡一段时间后彻底没反应"）
 *
 * 老版本只用 VersionReg(0x37) 判断芯片是否还活着，这是**不可靠**的判据：
 *   ① VersionReg 是只读硅片 ID（正片/克隆片各为 0x91/0x92/…）；
 *      **芯片哪怕掉电复位、把 Init 写进去的配置全丢了，它照样读得回这个 ID**；
 *   ② 芯片"数字域正常、射频/命令状态机卡死"时，VersionReg 同样正常。
 * 于是老逻辑常年走 else 分支只做一次 AntennaOn —— 芯片配置丢了根本补不回来，
 * 表现就是"读卡一会儿就再也没反应，只有断电才好"。
 *
 * 现在分两级：
 *   ① 寄存器回读自检：MFRC522_Check() 回读 Init 写过的 TModeReg(0x8D) /
 *      TPrescalerReg(0x3E)。读不回来说明芯片配置已丢（典型＝RC522 棕色复位），
 *      立即整片重初始化把配置补回去。
 *   ② 即便寄存器自检通过，也每 CARD_REINIT_AFTER_HEAL 次自愈无条件整片重初始化一次，
 *      兜底"射频/命令状态机卡死"这种读寄存器察觉不到的无形故障。
 *      重初始化只在**连续无卡（空闲）**时才发生，耗时 ~13 ms、场上无卡，安全。
 */
static void card_reader_heal(void)
{
    const bool regs_ok = (MFRC522_Check() != 0U);

    if ((!regs_ok) || (++s_card_heal_count >= CARD_REINIT_AFTER_HEAL)) {
        s_card_heal_count = 0U;
        printf("[RC522] heal: %s, re-init\r\n",
               regs_ok ? "periodic refresh" : "config lost");
        MFRC522_Init();
        if (!MFRC522_Check()) {
            printf("[RC522] WARN re-init failed -- check SCK/MOSI/MISO/PWR\r\n");
        }
    } else {
        MFRC522_AntennaOn();                /* 天线位在异常时可能被清掉 */
    }
}

static bool poll_card(board_input_event_t *event)
{
    uint8_t uid[FLASH_CARD_UID_SIZE];
    uint16_t user_id;
    const uint32_t now_ms = millis();

    /* 空闲时寻不到卡是正常态（天线区没卡），不要当错误 */
    if (!read_card_uid(uid)) {
        if (++s_card_heal_streak >= CARD_HEAL_AFTER_POLLS) {
            s_card_heal_streak = 0U;
            card_reader_heal();
        }
        return false;
    }
    s_card_heal_streak = 0U;
    s_card_heal_count = 0U;      /* 读到卡即清零自愈计数，周期刷新重新计时 */

    /* 同一张卡按住不放时去重，避免每 100 ms 一次地重复开锁 */
    if (s_last_card.valid && (memcmp(s_last_card.uid, uid, sizeof(uid)) == 0) &&
        ((now_ms - s_last_card.seen_at_ms) < CARD_DUPLICATE_MS)) {
        s_last_card.seen_at_ms = now_ms;
        return false;
    }
    (void)memcpy(s_last_card.uid, uid, sizeof(uid));
    s_last_card.seen_at_ms = now_ms;
    s_last_card.valid = true;

    /* 找登记条目：user_id 用于开锁记录，role 用于"管理卡免密进菜单" */
    user_id = 0U;
    s_card_last_role = BOARD_CARD_ROLE_NORMAL;
    {
        const int8_t idx = card_table_find_index(uid);
        if (idx >= 0) {
            user_id = s_card_table[idx].user_id;
            s_card_last_role = s_card_table[idx].role;
        }
    }
    if (user_id == 0U) {
#if SMART_LOCK_LEARN_FIRST_CARD
        if (s_card_count == 0U) {
            /* 首次上电自学习：把第一张卡登记为用户 1 */
            if (card_table_enroll(uid, 1U, BOARD_CARD_ROLE_NORMAL)) {
                user_id = 1U;
            }
        }
#endif
        if (user_id == 0U) {
            printf("[CARD] unknown uid %02X%02X%02X%02X\r\n",
                   uid[0], uid[1], uid[2], uid[3]);
            board_auth_feedback(false);
            return false;
        }
    }

    board_auth_feedback(true);
    (void)memset(event, 0, sizeof(*event));
    event->type = BOARD_AUTH_RFID;
    event->user_id = user_id;
    return true;
}

static bool poll_finger(board_input_event_t *event)
{
    uint16_t page = 0U;
    uint16_t score = 0U;

    /* AS608_Identify 返回 0 表示匹配成功；负数＝通信失败，其他为模块确认码 */
    if (AS608_Identify(&page, &score) != 0) {
        return false;
    }
    printf("[FINGER] page=%u score=%u\r\n", (unsigned)page, (unsigned)score);

    (void)memset(event, 0, sizeof(*event));
    event->digit_count = 0U;
    if (page == (uint16_t)AS608_DURESS_PAGE) {
        event->type = BOARD_AUTH_DURESS_FINGERPRINT; /* 胁迫指纹：照常开门，静默报警 */
        event->user_id = 0U;
    } else {
        event->type = BOARD_AUTH_FINGERPRINT;
        event->user_id = page; /* 页号即成员编号（1~48） */
    }
    board_auth_feedback(true);// 成功反馈
    return true;
}
// 轮询键盘输入、读头、菜单、唤醒键
bool board_poll_input(board_input_event_t *event)
{
    if (event == NULL) {
        return false;
    }

    //* 灯效与蜂鸣是 10 ms 步进的状态机，必须被周期驱动 
    buzzer_process();// 处理蜂鸣状态机
    rgb_process();// 处理 RGB 状态机

    // 处理提示行更新
    if (s_prompt_dirty) {// 提示行有更新
        s_prompt_dirty = false;
        board_ui_power(true);// 点亮屏幕
        ui_render_keypad_entry();// 重画提示行
    }

    /* --- 唤醒键 PA8（EXTI8）---
     * 中断只置了 s_wake_pending，这里把它变成一次 BOARD_TOUCH_WAKE：
     * 上层 access_task 收到后会让 ui_task 点屏并重绘待机界面。 */
    if (s_wake_pending) {
        s_wake_pending = false;
        buzzer_beep(BEEP_KEY);
        (void)memset(event, 0, sizeof(*event));
        event->type = BOARD_TOUCH_WAKE;
        return true;
    }

    /* --- 管理菜单模式（board_menu_set_active(true) 期间） ---
     * 按键逐个原样上抛（BOARD_MENU_KEY），不喂 keypad_input、不轮询读头。
     * 蜂鸣仍保留 —— 菜单里的每一次按键都要有可听反馈。 */
    if (s_menu_active) {
        const char key = keypad_scan_step(&s_matrix_io, &s_scan);//矩阵键盘“一次扫一列”的步进函数，10ms 一拍正好覆盖消抖
        if (key != KEYPAD_CHARACTER_NONE) {
            buzzer_beep(BEEP_KEY);
            (void)memset(event, 0, sizeof(*event));
            event->type = BOARD_MENU_KEY;
            event->digits[0] = key;
            event->digit_count = 1U;
            return true;
        }
        return false; /* 读头轮询（指纹/刷卡）在菜单期间整体让路 */
    }

    /* --- 键盘 --- */
    {
        const char key = keypad_scan_step(&s_matrix_io, &s_scan);
        if (key != KEYPAD_CHARACTER_NONE) {
            buzzer_beep(BEEP_KEY);
            //*把单个键喂进 PIN 缓冲，拼满/按 # / 按 B 等才产出 BOARD_AUTH_PIN 等完整事件
            if (keypad_input_feed(&s_keypad, key, millis(), event)) {
                if (event->type == BOARD_KEYPAD_CANCEL) {
                    board_ui_power(true);// 点亮屏幕
                    ui_render_keypad_entry();// 重画提示行
                }
                return true;
            }
            /* 数字、'*'、'C' 只进输入缓冲，本身不产生事件。若不在这里回显，
             * 用户看到的就只有一声 50 ms 的蜂鸣 —— 也就是"按键按了没反应"。
             * 点亮屏幕并把已输入位数显示出来，这是本地唯一的可见反馈。 */
            board_ui_power(true);
            ui_render_keypad_entry();// 重画提示行
            (void)memset(event, 0, sizeof(*event));
            event->type = BOARD_KEYPAD_ACTIVITY;
            return true;
        }
        if (keypad_input_tick(&s_keypad, millis())) {
            /* 无操作超时自动清空：把回显恢复成待机提示语 */
            ui_render_keypad_entry();
        }
    }

    ++s_poll_ticks;
    // 轮询读头（
    if ((s_poll_ticks % CARD_POLL_TICKS) == 0U) {
        if (poll_card(event)) {
            return true;
        }
    }
    // 轮询指纹传感器
    if ((s_poll_ticks % FINGER_POLL_TICKS) == 0U) {
        if (poll_finger(event)) {
            return true;
        }
    }
    return false;
}

/* ================================================================== */
/* 网络：上行物模型 + 下行桥                                            */
/* ================================================================== */

/*
 * 物模型里 method 的取值区间是 0~5（参考工程 app_wifi.c 的约束），
 * 而本工程的 lock_auth_method_t 有 0~7。这里做一次显式映射，
 * 保证不会把 >5 的值发给平台（否则平台回 2409 / 参数错误）。
 *   0 系统 SYSTEM  1 密码 PIN  2 刷卡 RFID  3 指纹 FINGERPRINT  4 动态口令 TOTP  5 远程 REMOTE
 */
static uint8_t thing_model_method(lock_auth_method_t method)
{
    switch (method) {
    case LOCK_METHOD_SYSTEM:      return 0U;
    case LOCK_METHOD_PIN:         return 1U;
    case LOCK_METHOD_RFID:        return 2U;
    case LOCK_METHOD_FINGERPRINT: return 3U;
    case LOCK_METHOD_TOTP:        return 4U;
    case LOCK_METHOD_REMOTE:      return 5U;
    case LOCK_METHOD_VISITOR:     return 1U; /* 访客码本质是密码，物模型无独立取值 */
    case LOCK_METHOD_PERIODIC:    return 1U; /* 周期密码同上 */
    default:                      return 0U;
    }
}

bool board_network_publish(const board_notification_t *notification)
{
    if (notification == NULL) {
        return false;
    }

    /* app_wifi.c 的 push 接口在离线时会静默丢弃，这里无需自己判在线状态 */
    switch (notification->event_type) {
        // 上报解锁事件
    case LOCK_EVENT_UNLOCK:
        app_wifi_push_door_event("unlock",
                                 thing_model_method(notification->auth_method));
        app_wifi_push_status();
        break;
        // 上报上锁事件
    case LOCK_EVENT_LOCK:
        app_wifi_push_door_event("lock",
                                 thing_model_method(notification->auth_method));
        app_wifi_push_status();
        break;
        // 上报胁迫事件
    case LOCK_EVENT_DURESS:
        /* 胁迫：门照常打开，但立即上报告警事件 + alarm_level */
        app_wifi_push_duress_event();
        app_wifi_push_alarm(ALARM_DURESS);
        break;
        // 上报篡改事件
    case LOCK_EVENT_TAMPER:
        app_wifi_push_alarm(ALARM_TAMPER);
        break;
        // 上报门打开事件
    case LOCK_EVENT_DOOR_AJAR:
        app_wifi_push_alarm(ALARM_DOOR_AJAR);
        break;
        // 上报门关闭事件
    case LOCK_EVENT_LOCKOUT:
        app_wifi_push_alarm(ALARM_LOCKOUT);
        break;
    case LOCK_EVENT_BOOT:
    case LOCK_EVENT_AUTH_FAILURE:
    case LOCK_EVENT_NETWORK:
    case LOCK_EVENT_STORAGE_FAILURE:
    case LOCK_EVENT_QUEUE_OVERFLOW:
    default:
        /* 物模型里没有对应属性或事件，平台会拒收，故不上报 */
        break;
    }
    return true;
}

bool board_network_poll_command(remote_command_t *command)
{
   // *处理接收数据
    app_wifi_process();// 处理 TCP 数据包

    if ((command == NULL) || (s_remote_head == s_remote_tail)) {
        return false;
    }
    *command = s_remote_ring[s_remote_tail];// 从环形缓冲区取命令
    s_remote_tail = (uint8_t)((s_remote_tail + 1U) % REMOTE_REQUEST_RING);// 更新尾指针
    return true;
}

/* ------------------------------------------------------------------ */
/* 安全配置持久化                                                      */
/* ------------------------------------------------------------------ */

static bool flash_read(void *context, uint32_t offset, uint8_t *data,
                       uint32_t length)
{
    (void)context;
    W25Q64_Read(offset, data, (uint16_t)length);
    return true;
}

static bool flash_write(void *context, uint32_t offset, const uint8_t *data,
                        uint32_t length)
{
    (void)context;
    W25Q64_PageProgram(offset, data, (uint16_t)length);
    return true;
}

static bool flash_erase(void *context, uint32_t address)
{
    (void)context;
    W25Q64_SectorErase(address);
    return true;
}

static const config_store_media_t s_config_media = {
    .context = NULL,
    .slot_a_address = FLASH_CONFIG_SLOT_A_ADDR,
    .slot_b_address = FLASH_CONFIG_SLOT_B_ADDR,
    .read = flash_read,
    .write = flash_write,
    .erase = flash_erase
};

/* 出厂默认配置：Flash 全 0xFF（全新芯片或已擦除）时写入 */
static void provision_defaults(board_security_config_t *config)
{
    static const uint8_t device_secret[BOARD_DEVICE_SECRET_SIZE] =
        PROVISION_DEFAULT_DEVICE_SECRET;
    static const uint8_t totp_secret[PROVISION_DEFAULT_TOTP_LENGTH] =
        PROVISION_DEFAULT_TOTP_SECRET;

    (void)memset(config, 0, sizeof(*config));
    (void)memcpy(config->device_secret, device_secret, sizeof(device_secret));
    (void)memcpy(config->totp_secret, totp_secret, sizeof(totp_secret));
    config->totp_secret_length = (uint8_t)PROVISION_DEFAULT_TOTP_LENGTH;

    (void)pin_credential_set(&config->owner_pin, device_secret,
                             sizeof(device_secret),
                             PROVISION_DEFAULT_OWNER_PIN,
                             sizeof(PROVISION_DEFAULT_OWNER_PIN) - 1U);
    (void)pin_credential_set(&config->duress_pin, device_secret,
                             sizeof(device_secret),
                             PROVISION_DEFAULT_DURESS_PIN,
                             sizeof(PROVISION_DEFAULT_DURESS_PIN) - 1U);

    config->periodic_pin.enabled = false;
    config->visitor.active = false;
    config->last_remote_request_id = 0U;
    config->second_factor_mode = BOARD_SECOND_FACTOR_DISABLED;
}

static void cache_config(const board_security_config_t *config)
{
    (void)memcpy(s_device_secret, config->device_secret,
                 sizeof(s_device_secret));
    s_remote_request_id = config->last_remote_request_id;
}

bool board_security_load(board_security_config_t *config)
{
    if (config == NULL) {
        return false;
    }

    if (!config_store_load(&s_config_media, config)) {
        /* 双槽都不可用 → 视为首次上电，写入出厂配置 */
        provision_defaults(config);
        if (!config_store_save(&s_config_media, config)) {
            board_diagnostic_report(BOARD_DIAG_SECURITY_STORAGE_FAILURE);
            return false;
        }
        printf("[CFG] provisioned factory defaults\r\n");
    }
    cache_config(config);
    return true;
}

bool board_security_save(const board_security_config_t *config)
{
    if (config == NULL) {
        return false;
    }
    if (!config_store_save(&s_config_media, config)) {
        return false;
    }
    cache_config(config);
    return true;
}

/* ================================================================== */
/* 执行器 / 声光反馈 / 界面                                            */
/* ================================================================== */

void board_lock_set(bool locked)
{
    if (locked) {
        servo_lock();
    } else {
        servo_unlock();
    }
    s_lock_engaged = locked;
    printf("[LOCK] %s\r\n", locked ? "locked" : "unlocked");
}

void board_alarm_set(bool active)
{
    if (active == s_alarm_active) {
        return;
    }
    s_alarm_active = active;
    if (active) {
        buzzer_beep(BEEP_ALARM);
        rgb_set_effect(RGB_EFFECT_ALARM);
    } else {
        buzzer_stop();
        rgb_set_effect(RGB_EFFECT_IDLE);
    }
}

void board_auth_feedback(bool success)
{
    if (success) {
        buzzer_beep(BEEP_OK);
        rgb_set_effect(RGB_EFFECT_OK);   /* 绿灯流水一圈 */
    } else {
        buzzer_beep(BEEP_ERR);
        rgb_set_effect(RGB_EFFECT_FAIL); /* 红灯闪三次 */
    }
}

void board_door_ajar_warning(void)
{
    /* 门虚掩：连响提示，并让 RGB 进入报警色提示 */
    buzzer_beep(BEEP_ERR);
    rgb_set_effect(RGB_EFFECT_ALARM);
    printf("[ALARM] door left open\r\n");
}

void board_ui_power(bool enabled)
{
    if (enabled) {
        OLED_DisplayOn();
    } else {
        OLED_DisplayOff();
    }
}

static const char *lock_state_text(lock_state_t state)
{
    switch (state) {
    case LOCK_STATE_LOCKED:   return "已上锁";
    case LOCK_STATE_UNLOCKED: return "已开锁";
    case LOCK_STATE_LOCKOUT:  return "错误锁定";
    case LOCK_STATE_ALARM:    return "报警中";
    default:                  return "未知";
    }
}

/*
 * 把一行文字用空格补满整屏宽度（128 px）后再送显。
 *
 * 为什么必须补：OLED_ShowText 只写它实际用到的像素，不负责擦掉上一次的内容。
 * 从"智能门锁 错误锁定"(120 px) 切到"开锁记录"(64 px) 时，右边 56 px 会留着
 * 上一次的字，看起来同样是"乱屏"。汉字 16 px、ASCII 8 px，128 减去任意组合
 * 宽度都一定是 8 的整数倍，所以补空格总能正好补到 128 px。
 */
static void ui_pad_line(char *line, size_t capacity)
{
    while ((OLED_TextWidth(line) < 128U) && ((strlen(line) + 1U) < capacity)) {
        (void)strcat(line, " ");
    }
}

static void ui_render_status(lock_state_t state, const char *message)
{
    char line[40];
    const uint64_t now = board_unix_time();
    DS3231_Time broken_down;

    /* 芯片/软时钟内部都是 UTC，显示用本地时间（北京时间） */
    DS3231_UnixToLocalTime((uint32_t)now, &broken_down);

    (void)snprintf(line, sizeof(line), "智能门锁 %s", lock_state_text(state));
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 0U, line);

    (void)snprintf(line, sizeof(line), "%02u-%02u %02u:%02u:%02u",
                   (unsigned)broken_down.month, (unsigned)broken_down.date,
                   (unsigned)broken_down.hour, (unsigned)broken_down.min,
                   (unsigned)broken_down.sec);
    /* ★ 2026-09-14 补齐整宽：时间行只有 88px，不补空格时从"待机"切到
     * 菜单页（该行内容更短）会在右侧留上一次的残影 —— 就是用户看到的
     * "OLED 重叠"。全部行统一 ui_pad_line 到 128px。 */
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 2U, line);

    /* 第 4 行有两个潜在写者：本函数（状态/提示消息）与 ui_render_keypad_entry()
     * （键盘回显）。改密码向导进行中时，s_prompt 不是 DEFAULT —— 此时这一行
     * 归向导独占，状态消息（"Access granted"、"Second factor expired" 之类）不能
     * 把它盖掉，否则用户刚看到"新密码"三个字就被一条无关消息顶没了。 */
    if (s_prompt != BOARD_PROMPT_DEFAULT) {
        (void)snprintf(line, sizeof(line), "%s", prompt_text(s_prompt));
    } else if (message != NULL) {
        (void)snprintf(line, sizeof(line), "%s", message);
    } else {
        (void)snprintf(line, sizeof(line), "请输入密码或指纹");
    }
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 4U, line);

    (void)snprintf(line, sizeof(line), "%s",
                   app_wifi_online() ? "云端已连接" : "云端未连接");
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 6U, line);
}

/*
 * 键盘回显：只重画第 4 行（提示行），把它换成"输入 ****"。
 *
 * 只画这一行是刻意的 —— 整屏的锁状态归应用层的 ui_render_status() 管，
 * 板级层不知道 LOCKOUT 之类的状态，越权重画会画出过期的界面。
 * 输入缓冲被超时清空时（count == 0）恢复成待机提示语。
 */
static const char *prompt_text(board_prompt_t prompt)
{
    switch (prompt) {
    case BOARD_PROMPT_OLD_PIN:     return "原密码";
    case BOARD_PROMPT_NEW_PIN:     return "新密码";
    case BOARD_PROMPT_CONFIRM_PIN: return "再输一遍";
    case BOARD_PROMPT_DEFAULT:
    default:                       return "请输入密码或指纹";
    }
}

void board_ui_set_prompt(board_prompt_t prompt)
{
    s_prompt = prompt;
    s_prompt_dirty = true;
}
// 重画提示行
static void ui_render_keypad_entry(void)
{
    char line[40];
    const uint8_t count = keypad_input_length(&s_keypad);
    const char *base;

    /* 待机态且已在输入时，沿用原来的短前缀"输入"，给星号多留些宽度；
     * 向导态（原密码/新密码/再输一遍）则直接用各自的分步提示语。 */
    if ((s_prompt == BOARD_PROMPT_DEFAULT) && (count > 0U)) {
        base = "输入";
    } else {
        base = prompt_text(s_prompt);
    }

    (void)snprintf(line, sizeof(line), "%s", base);
    if (count > 0U) {
        uint8_t i;
        (void)strcat(line, " ");
        for (i = 0U; i < count; ++i) {
            if ((OLED_TextWidth(line) + 8U) > 128U) {
                break; /* 再补一个 '*' 就超过 128 px 行宽了 */
            }
            (void)strcat(line, "*");
        }
    }
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 4U, line);
}

void board_ui_show(lock_state_t state, const char *message)
{
    ui_render_status(state, message);
}

static const char *event_type_text(uint8_t event_type)
{
    switch ((lock_event_type_t)event_type) {
    case LOCK_EVENT_UNLOCK:          return "开锁";
    case LOCK_EVENT_LOCK:            return "上锁";
    case LOCK_EVENT_AUTH_FAILURE:    return "验证失败";
    case LOCK_EVENT_LOCKOUT:         return "错误锁定";
    case LOCK_EVENT_TAMPER:          return "防撬报警";
    case LOCK_EVENT_DOOR_AJAR:       return "门虚掩";
    case LOCK_EVENT_NETWORK:         return "云端操作";
    case LOCK_EVENT_DURESS:          return "胁迫开锁";
    case LOCK_EVENT_STORAGE_FAILURE: return "存储故障";
    case LOCK_EVENT_QUEUE_OVERFLOW:  return "队列溢出";
    case LOCK_EVENT_BOOT:            return "开机";
    case LOCK_EVENT_CONFIG_CHANGE:   return "改密码";
    case LOCK_EVENT_ENROLL:          return "登记";
    case LOCK_EVENT_DELETE:          return "删除";
    default:                         return "未知";
    }
}

static const char *auth_method_text(uint8_t auth_method)
{
    switch ((lock_auth_method_t)auth_method) {
    case LOCK_METHOD_PIN:         return "密码";
    case LOCK_METHOD_FINGERPRINT: return "指纹";
    case LOCK_METHOD_RFID:        return "IC卡";
    case LOCK_METHOD_TOTP:        return "动态口令";
    case LOCK_METHOD_VISITOR:     return "访客码";
    case LOCK_METHOD_REMOTE:      return "远程";
    case LOCK_METHOD_PERIODIC:    return "周期密码";
    case LOCK_METHOD_SYSTEM:
    default:                      return "系统";
    }
}

void board_ui_show_log(const event_log_record_t *record)
{
    char line[40];
    DS3231_Time broken_down;

    if (record == NULL) {
        return;
    }
    /* 芯片/软时钟内部都是 UTC，显示用本地时间（北京时间） */
    DS3231_UnixToLocalTime((uint32_t)record->unix_time, &broken_down);

    (void)snprintf(line, sizeof(line), "开锁记录");
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 0U, line);

    (void)snprintf(line, sizeof(line), "%02u-%02u %02u:%02u:%02u",
                   (unsigned)broken_down.month, (unsigned)broken_down.date,
                   (unsigned)broken_down.hour, (unsigned)broken_down.min,
                   (unsigned)broken_down.sec);
    ui_pad_line(line, sizeof(line));      /* 整宽补齐，防残影（同上） */
    OLED_ShowText(0U, 2U, line);

    (void)snprintf(line, sizeof(line), "方式 %s",
                   auth_method_text(record->auth_method));
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 4U, line);

    (void)snprintf(line, sizeof(line), "%s  用户%u",
                   event_type_text(record->event_type), (unsigned)record->user_id);
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 6U, line);
}

/* ================================================================== */
/* 管理菜单：页面渲染 + 模式切换（2026-09-14 新增）                     */
/* ================================================================== */
/*
 * 与参考工程 app_ui.c 相同的按键习惯：
 *   菜单类页面  A=上移  C=下移  D=进入/确认  B=返回/退出
 *   输入类页面  数字输入  '#'=确认  B=删一位  C=退出
 * 所有中文常量必须留在本文件（字库按本文件的字面量扫描）。
 */

static const char *const menu_main_items[] = {
    "1:密码管理", "2:指纹管理", "3:卡片管理", "4:胁迫管理", "5:系统设置"
};
static const char *const menu_pin_items[] = {
    "1:修改主密码", "2:胁迫密码"
};
static const char *const menu_fp_items[] = {
    "1:录入指纹", "2:删除指纹", "3:管理员指纹"
};
static const char *const menu_card_items[] = {
    "1:添加卡片", "2:删除卡片", "3:添加管理卡"
};
static const char *const menu_duress_items[] = {
    "1:胁迫密码", "2:录入胁迫指纹", "3:删除胁迫指纹"
};
static const char *const menu_sys_items[] = {
    "1:修改时间", "2:开锁记录", "3:开锁组合"
};
static const char *const menu_combo_items[] = {
    "关:单因子", "1:指纹+密码", "2:刷卡+密码"
};

/* 画一个"标题 + 当前项 + 两行提示"的列表页（菜单/子菜单/组合开关共用）。
 * status != NONE 时，第 6 行短暂显示操作结果（替代常规提示行）。 */
static void menu_draw_list(const char *title, const char *const *items,
                           uint8_t count, uint8_t sel, uint8_t status,
                           const char *hint1, const char *hint2)
{
    char line[40];
    const char *bottom = hint2;

    switch (status) {
    case BOARD_MENU_STATUS_OK:    bottom = "操作成功"; break;
    case BOARD_MENU_STATUS_FAIL:  bottom = "操作失败"; break;
    case BOARD_MENU_STATUS_DUP:   bottom = "已存在";   break;
    case BOARD_MENU_STATUS_FULL:  bottom = "表已满";   break;
    case BOARD_MENU_STATUS_NOENT: bottom = "无此编号"; break;
    case BOARD_MENU_STATUS_SAVED: bottom = "已保存";   break;
    default: break; /* NONE → 用常规提示行 */
    }

    if (sel >= count) {
        sel = (uint8_t)(count - 1U);
    }
    (void)snprintf(line, sizeof(line), "%s", title);
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 0U, line);

    (void)snprintf(line, sizeof(line), "%s", items[sel]);
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 2U, line);

    (void)snprintf(line, sizeof(line), "%s", hint1);
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 4U, line);

    (void)snprintf(line, sizeof(line), "%s", bottom);
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 6U, line);
}

/* 画一个"标题 + 提示语 + 输入回显"的输入页（回显是应用层拼好的 ASCII） */
static void menu_draw_input(const char *title, const char *prompt,
                            const char *echo)
{
    char line[48];

    (void)snprintf(line, sizeof(line), "%s", title);
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 0U, line);

    if (echo != NULL) {
        (void)snprintf(line, sizeof(line), "%s %s", prompt, echo);
    } else {
        (void)snprintf(line, sizeof(line), "%s", prompt);
    }
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 2U, line);

    (void)snprintf(line, sizeof(line), "#确认 B删除");
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 4U, line);

    (void)snprintf(line, sizeof(line), "C退出");
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 6U, line);
}

/* 输入页的 aux → 标题/提示语（BOARD_MENU_INPUT_*） */
static void menu_input_texts(uint8_t aux, const char **title,
                             const char **prompt)
{
    switch (aux) {
    case BOARD_MENU_INPUT_NEW_PIN:        *title = "修改主密码"; *prompt = "新密码";   break;
    case BOARD_MENU_INPUT_CONFIRM:        *title = "修改主密码"; *prompt = "再输一遍"; break;
    case BOARD_MENU_INPUT_DURESS:         *title = "设置胁迫密码"; *prompt = "密码";  break;
    case BOARD_MENU_INPUT_DURESS_CONFIRM: *title = "设置胁迫密码"; *prompt = "再输一遍"; break;
    case BOARD_MENU_INPUT_FP_NUM:         *title = "录入指纹";   *prompt = "编号(1-47)"; break;
    case BOARD_MENU_INPUT_OLD_PIN:        *title = "修改主密码"; *prompt = "原密码";   break;
    case BOARD_MENU_INPUT_AUTH:
    default:                              *title = "管理员验证"; *prompt = "密码";   break;
    }
}

/* 指纹录入页：sel = 目标(0 普通 / 1 管理员 / 2 胁迫)，aux = 步骤 */
static void menu_draw_fp_enroll(uint8_t target, uint8_t step, const char *echo)
{
    const char *title;
    const char *status;
    char line[40];

    switch (target) {
    case 1U:  title = "管理员指纹"; break;
    case 2U:  title = "胁迫指纹";   break;
    default:  title = "录入指纹";   break;
    }
    switch (step) {
    case 1U:  status = "再按一次"; break;
    case 2U:  status = "处理中";   break;
    case 3U:  status = "已录入";   break;
    case 4U:  status = "录入失败"; break;
    default:  status = "请按手指"; break;
    }

    (void)snprintf(line, sizeof(line), "%s", title);
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 0U, line);

    (void)snprintf(line, sizeof(line), "%s", status);
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 2U, line);

    if (echo != NULL) {
        (void)snprintf(line, sizeof(line), "编号 %s", echo);
    } else {
        (void)snprintf(line, sizeof(line), "编号 --");
    }
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 4U, line);

    (void)snprintf(line, sizeof(line), "B取消");
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 6U, line);
}

/* 等刷卡页：sel = 模式(0 添加普通 / 1 添加管理卡 / 2 删除)，aux = 状态 */
static void menu_draw_card_wait(uint8_t mode, uint8_t status, const char *uid_hex)
{
    const char *title;
    const char *status_text;
    char line[40];

    switch (mode) {
    case 1U:  title = "添加管理卡"; break;
    case 2U:  title = "删除卡片";   break;
    default:  title = "添加卡片";   break;
    }
    switch (status) {
    case 1U:  status_text = "已登记"; break;
    case 2U:  status_text = "表已满"; break;
    case 3U:  status_text = "卡未登记"; break;
    case 4U:  status_text = "操作失败"; break;
    default:  status_text = "请刷卡"; break;
    }

    (void)snprintf(line, sizeof(line), "%s", title);
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 0U, line);

    if ((uid_hex != NULL) && (uid_hex[0] != '\0')) {
        (void)snprintf(line, sizeof(line), "ID:%s", uid_hex);
    } else {
        (void)snprintf(line, sizeof(line), "请刷卡");
    }
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 2U, line);

    (void)snprintf(line, sizeof(line), "%s", status_text);
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 4U, line);

    (void)snprintf(line, sizeof(line), "B取消");
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 6U, line);
}

void board_menu_show(board_menu_page_t page, uint8_t sel, uint8_t aux,
                     const char *line)
{
    const char *title = NULL;
    const char *prompt = NULL;

    switch (page) {
    case BOARD_MENU_MAIN:
        menu_draw_list("功能菜单", menu_main_items,
                       (uint8_t)(sizeof(menu_main_items) / sizeof(menu_main_items[0])),
                       sel, aux, "AC选择 D进入", "B退出");
        break;
    case BOARD_MENU_PIN:
        menu_draw_list("密码管理", menu_pin_items,
                       (uint8_t)(sizeof(menu_pin_items) / sizeof(menu_pin_items[0])),
                       sel, aux, "AC选择 D进入", "B返回");
        break;
    case BOARD_MENU_FP:
        menu_draw_list("指纹管理", menu_fp_items,
                       (uint8_t)(sizeof(menu_fp_items) / sizeof(menu_fp_items[0])),
                       sel, aux, "AC选择 D进入", "B返回");
        break;
    case BOARD_MENU_CARD:
        menu_draw_list("卡片管理", menu_card_items,
                       (uint8_t)(sizeof(menu_card_items) / sizeof(menu_card_items[0])),
                       sel, aux, "AC选择 D进入", "B返回");
        break;
    case BOARD_MENU_DURESS:
        menu_draw_list("胁迫管理", menu_duress_items,
                       (uint8_t)(sizeof(menu_duress_items) / sizeof(menu_duress_items[0])),
                       sel, aux, "AC选择 D进入", "B返回");
        break;
    case BOARD_MENU_SYS:
        menu_draw_list("系统设置", menu_sys_items,
                       (uint8_t)(sizeof(menu_sys_items) / sizeof(menu_sys_items[0])),
                       sel, aux, "AC选择 D进入", "B返回");
        break;
    case BOARD_MENU_COMBO:
        menu_draw_list("开锁组合", menu_combo_items,
                       (uint8_t)(sizeof(menu_combo_items) / sizeof(menu_combo_items[0])),
                       sel, aux, "AC选择 D确认", "B返回");
        break;
    case BOARD_MENU_TIME:
        menu_draw_input("修改时间", "YYMMDDHHMMSS", line);
        break;
    case BOARD_MENU_FP_DEL:
        menu_draw_input("删除指纹", "编号(1-49)", line);
        break;
    case BOARD_MENU_INPUT:
        menu_input_texts(aux, &title, &prompt);
        menu_draw_input(title, prompt, line);
        break;
    case BOARD_MENU_FP_ENROLL:
        menu_draw_fp_enroll(sel, aux, line);
        break;
    case BOARD_MENU_CARD_WAIT:
        menu_draw_card_wait(sel, aux, line);
        break;
    case BOARD_MENU_LOG:
    default:
        /* LOG 页走 board_menu_show_log()（需要完整日志记录），这里不画 */
        break;
    }
}

void board_menu_show_log(const event_log_record_t *record)
{
    char line[40];
    DS3231_Time broken_down;

    if (record == NULL) {
        /* 没有这一条记录：给一个空态页 */
        (void)snprintf(line, sizeof(line), "开锁记录");
        ui_pad_line(line, sizeof(line));
        OLED_ShowText(0U, 0U, line);
        (void)snprintf(line, sizeof(line), "无记录");
        ui_pad_line(line, sizeof(line));
        OLED_ShowText(0U, 2U, line);
        (void)snprintf(line, sizeof(line), "A上翻 C下翻");
        ui_pad_line(line, sizeof(line));
        OLED_ShowText(0U, 4U, line);
        (void)snprintf(line, sizeof(line), "B退出");
        ui_pad_line(line, sizeof(line));
        OLED_ShowText(0U, 6U, line);
        return;
    }
    /* 芯片/软时钟内部都是 UTC，显示用本地时间（北京时间） */
    DS3231_UnixToLocalTime((uint32_t)record->unix_time, &broken_down);

    (void)snprintf(line, sizeof(line), "开锁记录 AC翻");
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 0U, line);

    (void)snprintf(line, sizeof(line), "%02u-%02u %02u:%02u:%02u",
                   (unsigned)broken_down.month, (unsigned)broken_down.date,
                   (unsigned)broken_down.hour, (unsigned)broken_down.min,
                   (unsigned)broken_down.sec);
    ui_pad_line(line, sizeof(line));      /* 整宽补齐，防残影（同上） */
    OLED_ShowText(0U, 2U, line);

    (void)snprintf(line, sizeof(line), "方式 %s",
                   auth_method_text(record->auth_method));
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 4U, line);

    (void)snprintf(line, sizeof(line), "%s 用户%u B退",
                   event_type_text(record->event_type), (unsigned)record->user_id);
    ui_pad_line(line, sizeof(line));
    OLED_ShowText(0U, 6U, line);
}

void board_menu_set_active(bool active)
{
    if (s_menu_active == active) {
        return;
    }
    s_menu_active = active;
    if (active) {
        /* 进菜单：丢掉手上半截的 PIN 输入（否则退菜单后残留的数字会被
         * 当成登录密码的前几位），提示行也交回待机文案。 */
        keypad_input_init(&s_keypad);
        s_prompt = BOARD_PROMPT_DEFAULT;
        s_prompt_dirty = false;
    } else {
        /* 退菜单：恢复待机屏（下一次 ui 消息会重画全部四行） */
        keypad_input_init(&s_keypad);
        s_prompt = BOARD_PROMPT_DEFAULT;
        s_prompt_dirty = true;
    }
}

/* ------------------------------------------------------------------ */
/* 管理菜单用的卡片接口（录入 / 删除 / 角色查询）                       */
/* ------------------------------------------------------------------ */

bool board_card_read(uint8_t *uid)
{
    if (uid == NULL) {
        return false;
    }
    /* 与 poll_card() 共用同一套"重试 + HALT"读取层（菜单里添卡/删卡同理受益） */
    if (!read_card_uid(uid)) {
        return false;
    }
    /* 与 poll_card() 共用同一张"去重表"：同一张卡按住不放只算一次 */
    if (s_last_card.valid &&
        (memcmp(s_last_card.uid, uid, FLASH_CARD_UID_SIZE) == 0) &&
        ((millis() - s_last_card.seen_at_ms) < CARD_DUPLICATE_MS)) {
        s_last_card.seen_at_ms = millis();
        return false;
    }
    (void)memcpy(s_last_card.uid, uid, FLASH_CARD_UID_SIZE);
    s_last_card.seen_at_ms = millis();
    s_last_card.valid = true;
    return true;
}

bool board_card_add(const uint8_t *uid, uint8_t role, uint16_t user_id)
{
    if ((uid == NULL) || (role > BOARD_CARD_ROLE_ADMIN)) {
        return false;
    }
    if (card_table_find_index(uid) >= 0) {
        return false; /* 已登记过：不给重复登记 */
    }
    return card_table_enroll(uid, user_id, role);
}

bool board_card_remove(const uint8_t *uid)
{
    const int8_t idx = (uid != NULL) ? card_table_find_index(uid) : -1;

    if (idx < 0) {
        return false;
    }
    return card_table_remove_at((uint8_t)idx);
}

uint8_t board_card_count(void)
{
    return s_card_count;
}

uint8_t board_card_last_role(void)
{
    return s_card_last_role;
}

/* ================================================================== */
/* 审计日志：W25Q64 尾部每记录一个 4 KiB 扇区                          */
/* ================================================================== */

static uint32_t log_slot_address(uint32_t slot)
{
    return FLASH_LOG_BASE_ADDR + (slot * FLASH_SECTOR_SIZE);
}

bool board_log_read(void *context, uint32_t slot, event_log_record_t *record)
{
    (void)context;
    if ((record == NULL) || (slot >= (uint32_t)FLASH_LOG_SLOT_COUNT)) {
        return false;
    }
    W25Q64_Read(log_slot_address(slot), (uint8_t *)record,
                (uint16_t)sizeof(*record));
    return true;
}

bool board_log_write(void *context, uint32_t slot,
                     const event_log_record_t *record)
{
    (void)context;
    if ((record == NULL) || (slot >= (uint32_t)FLASH_LOG_SLOT_COUNT)) {
        return false;
    }
    /* 一次擦除 + 一次写入：event_log_append 已经先调用了 erase，
     * 这里只做编程，保证"断电最多丢这一条"的语义。 */
    W25Q64_PageProgram(log_slot_address(slot), (const uint8_t *)record,
                       (uint16_t)sizeof(*record));
    return true;
}

bool board_log_erase(void *context, uint32_t slot)
{
    (void)context;
    if (slot >= (uint32_t)FLASH_LOG_SLOT_COUNT) {
        return false;
    }
    W25Q64_SectorErase(log_slot_address(slot));
    return true;
}

/* ================================================================== */
/* 卡片 UID 表                                                         */
/* ================================================================== */

static bool card_table_load(void)
{
    uint8_t buffer[CARD_TABLE_HEADER +
                   (FLASH_CARD_TABLE_CAPACITY * CARD_TABLE_ENTRY_SIZE) + 4U];
    const uint32_t payload = CARD_TABLE_HEADER +
                             ((uint32_t)FLASH_CARD_TABLE_CAPACITY *
                              CARD_TABLE_ENTRY_SIZE);
    uint32_t stored_crc;
    uint8_t count;

    s_card_count = 0U;
    W25Q64_Read(FLASH_CARD_TABLE_ADDR, buffer, (uint16_t)sizeof(buffer));

    if ((buffer[0] != (uint8_t)(CARD_TABLE_MAGIC & 0xFFU)) ||
        (buffer[1] != (uint8_t)((CARD_TABLE_MAGIC >> 8U) & 0xFFU)) ||
        (buffer[2] != (uint8_t)((CARD_TABLE_MAGIC >> 16U) & 0xFFU)) ||
        (buffer[3] != (uint8_t)((CARD_TABLE_MAGIC >> 24U) & 0xFFU))) {
        return false; /* 未初始化过 → 当作空表 */
    }
    if (buffer[4] != (uint8_t)CARD_TABLE_VERSION) {
        printf("[CARD] unknown table version %u\r\n", (unsigned)buffer[4]);
        return false;
    }
    (void)memcpy(&stored_crc, &buffer[payload], sizeof(stored_crc));
    if (stored_crc != event_log_crc32(buffer, payload)) {
        printf("[CARD] table CRC mismatch, treated as empty\r\n");
        return false;
    }

    count = buffer[5];
    if (count > (uint8_t)FLASH_CARD_TABLE_CAPACITY) {
        return false;
    }
    for (uint8_t i = 0U; i < count; ++i) {
        const uint8_t *entry = &buffer[CARD_TABLE_HEADER +
                                       ((uint32_t)i * CARD_TABLE_ENTRY_SIZE)];
        (void)memcpy(s_card_table[i].uid, entry, FLASH_CARD_UID_SIZE);
        s_card_table[i].user_id =
            (uint16_t)((uint16_t)entry[4] | ((uint16_t)entry[5] << 8U));
        /* entry[6] = flags 字节：位0 = 管理员卡（旧表里是 0，正好 = 普通卡） */
        s_card_table[i].role = (uint8_t)(entry[6] & 0x01U);
    }
    s_card_count = count;
    printf("[CARD] %u card(s) loaded\r\n", (unsigned)s_card_count);
    return true;
}

static bool card_table_save(void)
{
    uint8_t buffer[CARD_TABLE_HEADER +
                   (FLASH_CARD_TABLE_CAPACITY * CARD_TABLE_ENTRY_SIZE) + 4U];
    const uint32_t payload = (uint32_t)CARD_TABLE_HEADER +
                             ((uint32_t)FLASH_CARD_TABLE_CAPACITY *
                              (uint32_t)CARD_TABLE_ENTRY_SIZE);
    uint32_t crc;

    /* 扇区布局：[0..3] magic | [4] version | [5] count | [6..7] 保留
     *           | [8..] 条目 | 末尾 4 字节 CRC32（覆盖到 CRC 之前） */
    (void)memset(buffer, 0, sizeof(buffer));
    buffer[0] = (uint8_t)(CARD_TABLE_MAGIC & 0xFFU);
    buffer[1] = (uint8_t)((CARD_TABLE_MAGIC >> 8U) & 0xFFU);
    buffer[2] = (uint8_t)((CARD_TABLE_MAGIC >> 16U) & 0xFFU);
    buffer[3] = (uint8_t)((CARD_TABLE_MAGIC >> 24U) & 0xFFU);
    buffer[4] = (uint8_t)CARD_TABLE_VERSION;
    buffer[5] = s_card_count;
    buffer[6] = 0U;
    buffer[7] = 0U;

    for (uint8_t i = 0U; i < s_card_count; ++i) {
        uint8_t *entry = &buffer[(uint32_t)CARD_TABLE_HEADER +
                                 ((uint32_t)i * (uint32_t)CARD_TABLE_ENTRY_SIZE)];
        (void)memcpy(entry, s_card_table[i].uid, FLASH_CARD_UID_SIZE);
        entry[4] = (uint8_t)(s_card_table[i].user_id & 0xFFU);
        entry[5] = (uint8_t)((s_card_table[i].user_id >> 8U) & 0xFFU);
        entry[6] = (uint8_t)(s_card_table[i].role & 0x01U); /* flags: 位0=管理员 */
        entry[7] = 0U;
    }

    crc = event_log_crc32(buffer, payload);
    (void)memcpy(&buffer[payload], &crc, sizeof(crc));

    W25Q64_SectorErase(FLASH_CARD_TABLE_ADDR);
    W25Q64_PageProgram(FLASH_CARD_TABLE_ADDR, buffer,
                       (uint16_t)(payload + (uint32_t)sizeof(crc)));
    return true;
}

/* ================================================================== */
/* 兼容垫片：app_wifi.c 需要的外部符号                                  */
/* ================================================================== */
/*
 * 为什么需要
 * ----------
 * 用户要求 ESP8266 的物模型报文与连接逻辑必须与参考工程**完全一致**
 * （OneNET 平台侧已按参考工程配置好物模型）。因此 USER/app_wifi.c 与
 * HARDWARE/esp8266.c 都是原样零改动复用，由本文件补齐它们引用的
 * app_* 钩子，把"云端语义"接到本工程的应用核心上。
 *
 * 下行信任链
 * ----------
 *   OneNET MQTT（用设备三元组换 token 鉴权）
 *     → app_wifi.c 解析 thing/property/set
 *       → 这里的钩子
 *         → 本地用设备密钥签一条 remote_command_t
 *           → board_network_poll_command() 交给 smart_lock_app.c
 *             → remote_command_verify() 仍然做**重放**与**过期**校验
 *
 * 说明：命令标签是本地生成的，所以"认证"这一步真正的信任根是 MQTT 的
 * token；本地签名的作用只是复用同一套命令结构与重放闸门。这是刻意为之：
 * 本产品是物模型产品，平台明确拒订 cmd/# 主题（SUBACK 0x80），
 * 不存在"平台直接下发已签名命令"的通道。
 */

/* 把一条待执行动作放进环形缓冲，并本地签名 */
static void cloud_enqueue(remote_action_t action, const char *visitor_code,
                          uint8_t visitor_code_length, uint8_t visitor_uses,
                          uint32_t ttl_seconds)
{
    remote_command_t command;
    uint8_t next_head = (uint8_t)((s_remote_head + 1U) % REMOTE_REQUEST_RING);

    if (next_head == s_remote_tail) {
        board_diagnostic_report(BOARD_DIAG_REMOTE_QUEUE_FULL);
        return;
    }

    ++s_remote_request_id;
    if (s_remote_request_id == 0U) {
        s_remote_request_id = 1U; /* 0 保留为"从未收到过" */
    }

    (void)memset(&command, 0, sizeof(command));
    command.request_id = s_remote_request_id;
    command.issued_at = board_unix_time();
    command.expires_at = command.issued_at + ttl_seconds;
    command.action = action;
    command.visitor_uses = visitor_uses;
    if ((visitor_code != NULL) && (visitor_code_length > 0U)) {
        const uint8_t length =
            (visitor_code_length > (uint8_t)sizeof(command.visitor_code))
                ? (uint8_t)sizeof(command.visitor_code)
                : visitor_code_length;
        (void)memcpy(command.visitor_code, visitor_code, length);
        command.visitor_code_length = length;
    }

    if (!remote_command_sign(&command, s_device_secret, sizeof(s_device_secret))) {
        board_diagnostic_report(BOARD_DIAG_SECURITY_STORAGE_FAILURE);
        return;
    }

    s_remote_ring[s_remote_head] = command;
    s_remote_head = next_head;
    printf("[CLOUD] queued action %u\r\n", (unsigned)action);
}

/* ---- 时钟与状态查询 ---- */

uint32_t app_now_unix(void)
{
    return (uint32_t)board_unix_time();
}

uint8_t app_lock_is_open(void)
{
    return s_lock_engaged ? 0U : 1U;
}

uint8_t app_auth_is_locked(void)
{
    return board_app_is_locked_out() ? 1U : 0U;
}

/* ---- 下行动作 ---- */

void app_auth_remote_unlock(void)
{
    cloud_enqueue(REMOTE_ACTION_UNLOCK, NULL, 0U, 0U, 30U);
}

void app_auth_remote_lock(void)
{
    cloud_enqueue(REMOTE_ACTION_LOCK, NULL, 0U, 0U, 30U);
}

void app_auth_remote_unlock_lockout(void)
{
    cloud_enqueue(REMOTE_ACTION_CLEAR_LOCKOUT, NULL, 0U, 0U, 30U);
}

uint8_t app_auth_add_temp_pwd(const char *pwd6, uint8_t onetime, uint32_t ttl_s)
{
    /*
     * 参考工程的临时密码表可放 4 条；本工程的 visitor_credential_t 是
     * "同一时刻只有一条有效临时码"的模型。这里按单条实现：
     *   - 已有生效中的临时码时返回 0（视为"表满"），平台会如实回执失败；
     *   - onetime != 0 → 只允许用 1 次；否则按次数上限 255 处理。
     * 这是刻意的取舍，避免让云端以为能下发多条却只生效一条。
     */
    if ((pwd6 == NULL) || (ttl_s == 0U)) {
        return 0U;
    }
    cloud_enqueue(REMOTE_ACTION_ISSUE_VISITOR, pwd6,
                  (uint8_t)strlen(pwd6),
                  (onetime != 0U) ? 1U : UINT8_MAX,
                  ttl_s);
    return 1U;
}

void app_log_add(uint8_t method, uint8_t user, uint8_t result)
{
    /* 不在这里写日志：上面 cloud_enqueue 出的命令被执行时，
     * smart_lock_app.c 已经会 queue_audit 记一条 LOCK_METHOD_REMOTE 记录。
     * 若此处再写会重复计数，因此只留调试输出。 */
    printf("[CLOUD] log method=%u user=%u result=%u\r\n",
           (unsigned)method, (unsigned)user, (unsigned)result);
}

void app_ui_notify(uint8_t event)
{
    if (event == (uint8_t)UI_EVENT_WIFI) {
        /* WiFi 状态变化：重绘一次待机界面，让"云端已连接"及时刷新 */
        ui_render_status(LOCK_STATE_LOCKED, NULL);
    }
}
