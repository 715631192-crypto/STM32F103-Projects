#ifndef __DS3231_H
#define __DS3231_H

#include "stm32f10x.h"

/*
 * DS3231 高精度实时时钟驱动（I2C，地址 0xD0）
 * ------------------------------------------------
 * 作用：
 *   1. 为 OLED 待机界面提供实时时间显示；
 *   2. 为 TOTP 动态口令提供可信时间戳（离线一次性密码的关键）；
 *   3. 为黑匣子日志提供"时间+人员+方式"中的时间字段。
 *   4. 为 OneNET token 的 et 过期时间戳提供时间基准。
 *
 * 内部寄存器（BCD 码）：
 *   0x00 秒  0x01 分  0x02 时  0x03 星期  0x04 日  0x05 月  0x06 年(00~99)
 *
 * ================= 时钟源选择（DS3231 未到货时的临时方案） =================
 *   DS3231_USE_SOFT = 1  → 软件 RTC：以"编译时间"为基准，靠 FreeRTOS 的
 *                         millis() 时基累加走时。上电复位后回到编译时间，
 *                         断电不保持。
 *   DS3231_USE_SOFT = 0  → 真实 DS3231 硬件。
 *
 * 模块到货后只需把下面的 1 改成 0，其余所有调用代码一行都不用动
 * （GetTime / SetTime / GetUnix / UnixToTime 接口完全一致）。
 *
 * ⚠️ 软 RTC 的两个已知限制：
 *   1. 复位/断电后回到编译时间 → 每次重新编译会刷新基准，够用；
 *      若隔天不重编就用 DS3231_SoftSetUnix() 授时（可接 NTP 或串口命令）。
 *   2. Stop 低功耗期间 SysTick 停止 → 软时钟停走，
 *      因此 app_power.c 在软模式下会禁止进入 Stop。
 */

#define DS3231_ADDR   0xD0

/* ★ 时钟源选择（2026-09-15 改为使用真实 DS3231）
 *   DS3231_USE_SOFT = 0  → **优先真实 DS3231 硬件**：
 *                          软件时钟仍保留，仅当芯片不应答（没接/没电/坏）时
 *                          自动回落到"编译时间基准"，保证 TOTP、日志、
 *                          云端 token 的时间轴自洽（串口会打印回退告警）。
 *   DS3231_USE_SOFT = 1  → 强制软件 RTC（调试用，完全不碰 I2C）。
 *
 * ⚠ 硬件模式下的时间语义：**芯片里存的是 UTC**。
 *   DS3231_GetTime / GetUnix 返回 UTC（TOTP、OneNET token 的 et、锁定时效
 *   都按 UTC 计算）；OLED 待机时钟与"开锁记录"显示的是**本地时间**
 *   （+DS3231_LOCAL_TZ_HOURS），菜单"修改时间"输入的也是本地时间，写芯片
 *   前会自动换算成 UTC。
 *
 * ⚠ 软时钟的两个已知限制（只在回退时生效）：
 *   1. 复位/断电后回到编译时间 → 每次重新编译会刷新基准；
 *      长期挂机请依赖 DS3231（有纽扣电池，断电不丢）。
 *   2. Stop 低功耗期间 SysTick 停止 → 软时钟停走。
 */
#define DS3231_USE_SOFT       0

/* 编译时间的时区（小时）。编译机是北京时间，故 +8。
 * 仅软时钟用它把"编译时刻"折成 UTC（真实 DS3231 不需要）。 */
#define DS3231_BUILD_TZ_HOURS 8

/* 本地时区（小时）：只用于**显示与输入**换算，内部一律 UTC。
 * 8 = 北京时间。OLED 时钟、开锁记录、菜单改时间都用它。 */
#define DS3231_LOCAL_TZ_HOURS 8

/* 时间结构体：均为二进制数（驱动内部完成 BCD 换算） */
typedef struct
{
    uint8_t year;     /* 0~99 → 2000~2099 */
    uint8_t month;    /* 1~12   */
    uint8_t date;     /* 1~31   */
    uint8_t week;     /* 1=周一 ... 7=周日 */
    uint8_t hour;     /* 0~23   */
    uint8_t min;      /* 0~59   */
    uint8_t sec;      /* 0~59   */
} DS3231_Time;

uint8_t DS3231_Init(void);
void    DS3231_GetTime(DS3231_Time *t);
void    DS3231_SetTime(const DS3231_Time *t);
uint32_t DS3231_GetUnix(DS3231_Time *t);   /* 换算为 Unix 时间戳(秒) */
void    DS3231_UnixToTime(uint32_t unix, DS3231_Time *t); /* 反算(日志显示用) */

/* ---- 本地时间换算（2026-09-15 新增）----
 * 内部时间轴一律 UTC；下面两个只服务于"给人看/由人输"的两处：
 *   DS3231_UnixToLocalTime()   OLED 待机时钟 / 开锁记录显示（+LOCAL_TZ）
 *   DS3231_LocalTimeToUtcUnix() 菜单"修改时间"输入（−LOCAL_TZ）
 */
void     DS3231_UnixToLocalTime(uint32_t utc_unix, DS3231_Time *t);
uint32_t DS3231_LocalTimeToUtcUnix(const DS3231_Time *local_time);

/* 1 = 真实 DS3231 在线；0 = 已回落到软件时钟（芯片没应答） */
uint8_t  DS3231_HardwarePresent(void);

/* 软时钟专用：直接用 Unix 时间戳授时。
 * 后续若接 NTP（AT+CIPSNTPTIME?）或串口命令，把解析出的时间戳丢进来即可。 */
void    DS3231_SoftSetUnix(uint32_t unix_time);
/* 读取软时钟当前 Unix 时间（调试/打印用） */
uint32_t DS3231_SoftNowUnix(void);

#endif
