#ifndef SMART_LOCK_BOARD_CONFIG_H
#define SMART_LOCK_BOARD_CONFIG_H

/*
 * 板级「可移植」配置：时序、容量、存储分区、平台主题。
 *
 * 这里**只放不依赖 stm32f10x.h 的常量**，因此 smart_lock_app.c 在主机
 * 桩件构建里也能包含本文件。真正的 GPIO 引脚表在 smart_lock_pinmap.h，
 * 那个头文件需要 stm32f10x.h，只有板级适配层（board_port.c / main.c /
 * stm32f10x_it.c）才包含它。
 *
 * 硬件基准：STM32F103C8T6，LQFP48，HSE 8 MHz，SYSCLK 72 MHz。
 */

/* ------------------------------------------------------------------ */
/* 时钟                                                                */
/* ------------------------------------------------------------------ */
#define SMART_LOCK_SYSCLK_HZ            72000000UL
#define SMART_LOCK_APB1_HZ              36000000UL
#define SMART_LOCK_APB2_HZ              72000000UL

/* ------------------------------------------------------------------ */
/* 串口波特率                                                          */
/* ------------------------------------------------------------------ */
#define ESP8266_UART_BAUD                 115200UL  /* USART1 */
#define AS608_UART_BAUD                    57600UL  /* USART2，ZW101/HLK-ZW101 */
#define DEBUG_UART_BAUD                   115200UL  /* USART3，printf 调试口 */

/* ------------------------------------------------------------------ */
/* 器件地址                                                            */
/* ------------------------------------------------------------------ */
#define OLED_I2C_ADDRESS_8BIT                0x78U  /* SSD1306 8 位写地址 */
#define DS3231_I2C_ADDRESS_8BIT              0xD0U  /* DS3231  8 位写地址 */

/* 软件 I2C 位延时（NOP 循环次数），约 100 kHz 级别；太快会挑线长 */
#define SOFT_I2C_DELAY_LOOPS                    4U

/* ------------------------------------------------------------------ */
/* 舵机（SG90，TIM3_CH3 → PB0）                                        */
/* ------------------------------------------------------------------ */
#define SERVO_PWM_PERIOD_US                 20000UL  /* 50 Hz */
#define SERVO_LOCKED_PULSE_US                 500UL  /* 0°   = 上锁位 */
#define SERVO_UNLOCKED_PULSE_US              1500UL  /* 90°  = 开锁位 */
#define SERVO_MIN_ANGLE                         0U
#define SERVO_MAX_ANGLE                       180U
#define AUTO_LOCK_DELAY_SECONDS                 5UL  /* 开锁后自动回锁 */

/* ------------------------------------------------------------------ */
/* 交互时序                                                            */
/* ------------------------------------------------------------------ */
#define KEYPAD_DEBOUNCE_MS                     25UL  /* 两拍确认消抖 */
#define KEYPAD_INACTIVITY_TIMEOUT_MS         8000UL  /* 无操作清零输入缓冲区 */
#define STOP_MODE_DELAY_SECONDS                60UL  /* 静止多久进 STOP */
#define DOOR_AJAR_DELAY_SECONDS                30UL  /* 门开多久报虚掩 */
#define SECOND_FACTOR_TIMEOUT_SECONDS          15UL  /* 双因子第二步限时 */
#define PIN_WIZARD_TIMEOUT_SECONDS             60UL  /* 改主密码向导整体限时：
                                                      * 从进向导开始计，任一步都
                                                      * 不能超过这个时长，避免用户
                                                      * 输一半就走人，把向导永久
                                                      * 挂在"待输入新密码"状态 */

/* 灯效/蜂鸣以 10 ms 为节拍推进，任务周期必须与之对齐 */
#define FEEDBACK_TICK_MS                       10UL

/* ------------------------------------------------------------------ */
/* 存储布局（W25Q64，8 MiB = 2048 个 4 KiB 扇区）                      */
/* ------------------------------------------------------------------ */
/* 配置区采用 A/B 双槽 + 版本号 + CRC32 信封，保证掉电不丢配置。 */
#define FLASH_CONFIG_SLOT_A_ADDR       0x000000UL
#define FLASH_CONFIG_SLOT_B_ADDR       0x001000UL
#define FLASH_CONFIG_SECTOR_SIZE          4096UL

/* 卡片 UID 表（IC 卡白名单） */
#define FLASH_CARD_TABLE_ADDR          0x002000UL
#define FLASH_CARD_TABLE_CAPACITY            32U   /* 最多 32 张卡 */
#define FLASH_CARD_UID_SIZE                   4U   /* MFRC522 一级防冲撞只有 4 字节 */

/* 审计日志环：从尾部倒推 EVENT_LOG_SLOT_COUNT 个扇区，每扇区一条记录。
 * 浪费容量，但「一次擦除 + 一次写入」的掉电语义最容易验证。 */
#define FLASH_LOG_SLOT_COUNT                 128U
#define FLASH_TOTAL_SECTORS                 2048UL
#define FLASH_SECTOR_SIZE                 4096UL
#define FLASH_LOG_BASE_ADDR \
    ((FLASH_TOTAL_SECTORS - (unsigned long)FLASH_LOG_SLOT_COUNT) * FLASH_SECTOR_SIZE)

/* 第一启动（Flash 全 0xFF）时写入的出厂配置。 */
#define PROVISION_DEFAULT_OWNER_PIN        "123456"
#define PROVISION_DEFAULT_DURESS_PIN       "654321"
/* 32 字节设备密钥：正式发布前必须逐台改写，且不得提交到版本库。 */
#define PROVISION_DEFAULT_DEVICE_SECRET \
    { 0x8F, 0x24, 0xC1, 0x7A, 0x03, 0x9D, 0x56, 0xE8, \
      0x11, 0xB7, 0x4C, 0xD2, 0x6A, 0x35, 0xF0, 0x9C, \
      0x5E, 0x82, 0xAB, 0x17, 0x40, 0xC9, 0x7B, 0x2D, \
      0x63, 0x08, 0xE5, 0x94, 0x3A, 0x7F, 0xD6, 0x51 }
/* TOTP 共享密钥长度必须落在 [16, 32] 字节，供 RFC 6238 使用。 */
#define PROVISION_DEFAULT_TOTP_LENGTH          20U
#define PROVISION_DEFAULT_TOTP_SECRET \
    { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, \
      0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30 }

/* ------------------------------------------------------------------ */
/* 功能开关                                                            */
/* ------------------------------------------------------------------ */
/* 门磁(PC13) 与防撬(PC14) **暂不启用**：
 *   - PC14 已被 MFRC522 的 RST 硬复位占用（见 mfrc522.c 的 RC522_RST_H/L）；
 *   - PC13 门磁硬件也未接，悬空脚配上拉输入会产生虚假中断。
 * 因此板级不注册 EXTI13/EXTI15_10，board_poll_input 也不产生
 * BOARD_DOOR_OPENED / BOARD_DOOR_CLOSED / BOARD_TAMPER_TRIGGERED 事件。
 * 上层 app 对这些事件的处理逻辑保持完整，日后接线只需把下表置 1
 * 并补上 GPIO/EXTI 初始化即可，无需改动应用层。 */
#define SMART_LOCK_HAS_DOOR_SENSOR              0
#define SMART_LOCK_HAS_TAMPER_SENSOR            0

/* 独立看门狗：默认关闭，便于调试与烧录；发布版置 1。
 * 开启后 board_watchdog_refresh() 才会真正喂狗。 */
#define SMART_LOCK_ENABLE_IWDG                  0
#define SMART_LOCK_IWDG_RELOAD                  3125U  /* LSI 40kHz / 256 分频 → 约 2 s */

/* STOP 低功耗：默认关闭。docs/bring-up.md 要求先验证唤醒与时钟恢复，
 * 确认 UART/I2C 恢复后不失步再置 1。关闭时 board_try_stop_mode() 返回
 * false，表示"没有真的睡下去"，上层会照常刷新活跃时间。 */
#define SMART_LOCK_ENABLE_STOP_MODE             0

/* 首次上电（卡片表为空）时，把第一张刷到的卡登记为用户 1 的自学习开关。
 * 便于现场装卡；正式出货的产测流程应改为按键菜单录入并把本项置 0。 */
#define SMART_LOCK_LEARN_FIRST_CARD             1

/* ------------------------------------------------------------------ */
/* OneNET Studio 物模型（标识符与主题必须与参考工程完全一致）           */
/* ------------------------------------------------------------------ */
/* 注意：WiFi 名称/密码与 OneNET 三元组（ProductID / DeviceName /
 * DeviceSecret）统一由 `HARDWARE/esp8266.h` 提供，此项目直接沿用，
 * 保证「WiFi 密码与 token 与参考工程一致」。本文件不重复定义它们。 */
#define ONENET_MODEL_PROP_POST_TMPL   "$sys/%s/%s/thing/property/post"
#define ONENET_MODEL_PROP_SET_TMPL    "$sys/%s/%s/thing/property/set"
#define ONENET_MODEL_PROP_REPLY_TMPL  "$sys/%s/%s/thing/property/set_reply"
#define ONENET_MODEL_SRV_INVOKE_TMPL  "$sys/%s/%s/thing/service/+/invoke"
#define ONENET_MODEL_EVENT_POST_TMPL  "$sys/%s/%s/thing/event/post"

/* 物模型属性标识符 */
#define ONENET_MODEL_PROP_LOCK_STATUS  "lock_status"   /* bool，true=已开 */
#define ONENET_MODEL_PROP_DOOR_STATUS  "door_status"   /* bool，true=开 */
#define ONENET_MODEL_PROP_ALARM_LEVEL  "alarm_level"   /* string */

/* 物模型告警等级取值 */
#define ONENET_MODEL_ALARM_NONE        "none"
#define ONENET_MODEL_ALARM_TAMPER      "tamper"
#define ONENET_MODEL_ALARM_DURESS      "duress"
#define ONENET_MODEL_ALARM_DOOR_AJAR   "door_ajar"

/* 物模型事件标识符（APP「日志」页数据源） */
#define ONENET_MODEL_EVENT_DOOR        "door_event"
#define ONENET_MODEL_EVENT_DURESS      "duress_scene"

/* 事件参数取值上限：物模型里 method 的区间是 0~5 */
#define ONENET_MODEL_METHOD_MAX                 5U

/* 设备上行报文缓冲长度 */
#define ONENET_MODEL_PAYLOAD_MAX              384U
#define ONENET_MODEL_TOPIC_MAX                128U

#endif /* SMART_LOCK_BOARD_CONFIG_H */
