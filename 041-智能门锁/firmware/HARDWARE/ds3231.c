#include "ds3231.h"
#include "i2c_soft.h"
#include "delay.h"          /* millis()：软件 RTC 的走时基准 */
#include <stdbool.h>
#include <stdio.h>          /* 回退告警打印 */

/* ================================================================
 *  一、真实 DS3231 硬件（DS3231_USE_SOFT = 0 时为**首选**时钟源）
 *      探测失败（芯片没接/没电/坏）时自动回落到第三节的软时钟
 * ================================================================ */
#if !DS3231_USE_SOFT

/* 1 = 芯片在线，所有读写都走 I2C；0 = 已回落到软时钟 */
static bool s_rtc_hw_ok;

/* ---------------- BCD 与二进制互转 ----------------
 * DS3231 寄存器以 BCD 码存储：例如 23 存为 0x23
 */
static uint8_t bcd2bin(uint8_t b) { return (uint8_t)((b >> 4) * 10 + (b & 0x0F)); }
static uint8_t bin2bcd(uint8_t b) { return (uint8_t)(((b / 10) << 4) | (b % 10)); }

/** @brief 总线可用性探测：试读 7 个时间寄存器 */
static uint8_t hw_probe(void)
{
    uint8_t buf[7];
    return IIC_ReadRegs(DS3231_ADDR, 0x00, buf, 7);
}

/** @brief 从芯片读时间（自动 BCD→二进制）。成功返回 1 */
static uint8_t hw_read(DS3231_Time *t)
{
    uint8_t buf[7];
    if (!IIC_ReadRegs(DS3231_ADDR, 0x00, buf, 7))
        return 0;                            /* 读取失败：由调用方处理 */
    t->sec   = bcd2bin(buf[0] & 0x7F);       /* 掩掉 CH 位        */
    t->min   = bcd2bin(buf[1] & 0x7F);
    t->hour  = bcd2bin(buf[2] & 0x3F);       /* 掩掉 12/24 模式位 */
    t->week  = bcd2bin(buf[3] & 0x07);
    t->date  = bcd2bin(buf[4] & 0x3F);
    t->month = bcd2bin(buf[5] & 0x1F);       /* 掩掉世纪位        */
    t->year  = bcd2bin(buf[6] & 0xFF);
    return 1;
}

/** @brief 写时间到芯片（自动二进制→BCD） */
static void hw_write(const DS3231_Time *t)
{
    IIC_WriteReg(DS3231_ADDR, 0x00, bin2bcd(t->sec));
    IIC_WriteReg(DS3231_ADDR, 0x01, bin2bcd(t->min));
    IIC_WriteReg(DS3231_ADDR, 0x02, bin2bcd(t->hour));
    IIC_WriteReg(DS3231_ADDR, 0x03, bin2bcd(t->week));
    IIC_WriteReg(DS3231_ADDR, 0x04, bin2bcd(t->date));
    IIC_WriteReg(DS3231_ADDR, 0x05, bin2bcd(t->month));
    IIC_WriteReg(DS3231_ADDR, 0x06, bin2bcd(t->year));
}

#endif /* !DS3231_USE_SOFT */


/* ================================================================
 *  二、日历 ⇄ Unix 换算（纯算法，两种时钟源共用）
 * ================================================================ */

/**
 * @brief 把日历时间换算为 Unix 时间戳（秒，1970 起）
 *        采用 Howard Hinnant 的 civil 算法（2000~2099 有效），
 *        TOTP 动态口令的"时间片"就来自这里。
 *
 * ⚠️ 修正记录：校正常量原为 719528，会导致算出的 Unix 时间
 *    恒定偏小 5184000 秒（整整 60 天）。后果是
 *      - TOTP 时间片完全错位，动态口令永远对不上；
 *      - OneNET token 的 et 偏早 60 天 → token 一生成就过期，设备连不上云。
 *    正确值与下面的 DS3231_UnixToTime() 互为逆运算，均为 719468。
 */
uint32_t DS3231_GetUnix(DS3231_Time *t)
{
    uint32_t y = 2000 + t->year;
    uint32_t m = t->month;
    uint32_t d = t->date;
    uint32_t days;

    y -= (m <= 2);                            /* 1/2 月算作上一年 13/14 月 */
    days = (365 * y) + (y / 4) - (y / 100) + (y / 400);
    days += (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    days -= 719468;                           /* 校正到 1970-01-01 起 */

    return days * 86400UL
         + (uint32_t)t->hour * 3600UL
         + (uint32_t)t->min * 60UL
         + (uint32_t)t->sec;
}

/**
 * @brief Unix 时间戳反算日历时间（Howard Hinnant civil 算法逆变换）
 *        用于把黑匣子日志里的时间戳还原成可读的 年月日时分秒
 */
void DS3231_UnixToTime(uint32_t unix, DS3231_Time *t)
{
    uint32_t days = unix / 86400UL;
    uint32_t rem  = unix % 86400UL;
    uint32_t era, doe, yoe, y, doy, mp, m, d;

    /* 时分秒 */
    t->hour = (uint8_t)(rem / 3600UL);
    t->min  = (uint8_t)((rem / 60UL) % 60UL);
    t->sec  = (uint8_t)(rem % 60UL);

    /* 日 → 年月日（civil_from_days） */
    days += 719468UL;
    era = days / 146097UL;                     /* 400 年纪元       */
    doe = days % 146097UL;                     /* 纪元内第几天     */
    yoe = (doe - doe / 1460UL + doe / 36524UL - doe / 146096UL) / 365UL;
    y   = yoe + era * 400UL;
    doy = doe - (365UL * yoe + yoe / 4UL - yoe / 100UL);
    mp  = (5UL * doy + 2UL) / 153UL;
    d   = doy - (153UL * mp + 2UL) / 5UL + 1UL;
    m   = mp + (mp < 10UL ? 3UL : (uint32_t)-9UL);
    if (m <= 2UL)
        y++;

    t->year  = (uint8_t)(y - 2000UL);
    t->month = (uint8_t)m;
    t->date  = (uint8_t)d;

    /* 星期：1970-01-01 是周四；结果 1=周一 ... 7=周日 */
    {
        uint32_t wd = (unix / 86400UL + 3UL) % 7UL + 1UL;
        t->week = (uint8_t)wd;
    }
}


/* ================================================================
 *  三、软件 RTC（**总是编译**）
 *
 *  DS3231_USE_SOFT = 1 → 唯一时钟源（调试用，完全不碰 I2C）
 *  DS3231_USE_SOFT = 0 → 仅作真实芯片探测失败时的**回退**：
 *      基准时间 = 编译时间（__DATE__ / __TIME__）
 *      走    时 = FreeRTOS 的 millis() 时基累加
 *
 *  接口在第四节的 DS3231_* 里统一分派，调用方完全不用关心用的是哪一种。
 * ================================================================ */

/* 软时钟状态：基准 Unix 时间 + 设定基准时的 millis() 值 */
static uint32_t s_base_unix = 0;
static uint32_t s_base_ms   = 0;

/** @brief 解析 __DATE__ 里的英文月份缩写 → 1~12 */
static uint8_t soft_month(const char *s)
{
    static const char names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    uint8_t i;
    for (i = 0; i < 12; i++)
    {
        if (s[0] == names[i * 3] &&
            s[1] == names[i * 3 + 1] &&
            s[2] == names[i * 3 + 2])
        {
            return (uint8_t)(i + 1);
        }
    }
    return 1;                       /* 解析不出来就退回 1 月，保证程序能跑 */
}

/** @brief 取两位十进制；前导空格按 0 处理（__DATE__ 的日期可能是 " 9"） */
static uint8_t soft_2d(const char *s)
{
    uint8_t v = 0;
    if (s[0] >= '0' && s[0] <= '9')
        v = (uint8_t)((s[0] - '0') * 10);
    return (uint8_t)(v + (uint8_t)(s[1] - '0'));
}

/**
 * @brief 由编译期宏 __DATE__ / __TIME__ 得到 Unix 基准时间
 *   __DATE__ 形如 "Sep  9 2026"（下标 0~10，日不足两位时前导空格）
 *   __TIME__ 形如 "15:18:32"（下标 0~7）
 *   二者都是【本地时间】，转 Unix 时要减掉时区偏移
 */
static uint32_t soft_build_unix(void)
{
    DS3231_Time bt;
    uint32_t    u;

    bt.month = soft_month(__DATE__);
    bt.date  = soft_2d(&__DATE__[4]);                       /* 下标 4~5 */
    bt.year  = (uint8_t)((__DATE__[9] - '0') * 10 +
                         (__DATE__[10] - '0'));             /* 下标 9~10 → 26 */
    bt.hour  = soft_2d(&__TIME__[0]);
    bt.min   = soft_2d(&__TIME__[3]);
    bt.sec   = soft_2d(&__TIME__[6]);
    bt.week  = 1;                       /* 占位，读取时由 UnixToTime 反算覆盖 */

    u = DS3231_GetUnix(&bt);
    u -= (uint32_t)DS3231_BUILD_TZ_HOURS * 3600UL;          /* 北京 UTC+8 → UTC */
    return u;
}

/** @brief 软时钟"起表"：装上编译时间作为基准（硬件模式回退时调用） */
static void soft_start(void)
{
    s_base_unix = soft_build_unix();
    s_base_ms   = millis();
}

/** @brief 当前 Unix 时间 = 基准 + 已运行的秒数 */
uint32_t DS3231_SoftNowUnix(void)
{
    /* millis() 约 49.7 天回绕，有符号减法天然正确处理（只要间隔 < 24.8 天）。
     * 用有符号：调度器启动瞬间 tick 从 DWT 兜底值跳回 0，
     * 差值短暂为负只会让时钟滞后几秒，随后正常走时；无符号会瞬间溢出 +49.7 天 */
    return s_base_unix + (uint32_t)((int32_t)(millis() - s_base_ms) / 1000);
}

/** @brief 授时入口：接 NTP / 串口命令时把解析出的时间戳丢进来 */
void DS3231_SoftSetUnix(uint32_t unix_time)
{
    s_base_unix = unix_time;
    s_base_ms   = millis();
}


/* ================================================================
 *  四、对外接口（两种时钟源统一在这里分派）
 * ================================================================ */

/**
 * @brief 时钟初始化
 * @return 1 = 真实 DS3231 在线（首选）；0 = 已回落到软件时钟
 */
uint8_t DS3231_Init(void)
{
#if DS3231_USE_SOFT
    soft_start();
    return 1;                        /* 强制软时钟：永远"在线" */
#else
    if (hw_probe())
    {
        s_rtc_hw_ok = true;
        return 1;
    }
    /* 芯片没应答：回落到编译时间基准，保证 TOTP / 日志 / 云端 token
     * 的时间轴仍然自洽（否则时间会停在 1970 年，动态码、token 全废）。 */
    s_rtc_hw_ok = false;
    soft_start();
    printf("[RTC] DS3231 no ACK -> fallback to soft clock\r\n");
    return 0;
#endif
}

/** @brief 读当前时间（返回 UTC；芯片与软时钟都适用） */
void DS3231_GetTime(DS3231_Time *t)
{
    if (!t) return;
#if DS3231_USE_SOFT
    DS3231_UnixToTime(DS3231_SoftNowUnix(), t);
#else
    if (s_rtc_hw_ok && hw_read(t))
        return;
    DS3231_UnixToTime(DS3231_SoftNowUnix(), t);   /* 芯片掉线 → 用软时钟兜底 */
#endif
}

/** @brief 设置时间（入参是 **UTC**） */
void DS3231_SetTime(const DS3231_Time *t)
{
    DS3231_Time tmp;
    if (!t) return;
    tmp = *t;
#if DS3231_USE_SOFT
    DS3231_SoftSetUnix(DS3231_GetUnix(&tmp));
#else
    if (s_rtc_hw_ok)
    {
        hw_write(&tmp);              /* 写进芯片，纽扣电池维持 */
    }
    else
    {
        DS3231_SoftSetUnix(DS3231_GetUnix(&tmp));
    }
#endif
}

/** @brief UTC Unix → 本地日期时间（给 OLED 显示用） */
void DS3231_UnixToLocalTime(uint32_t utc_unix, DS3231_Time *t)
{
    if (!t) return;
    DS3231_UnixToTime(utc_unix + (uint32_t)DS3231_LOCAL_TZ_HOURS * 3600UL, t);
}

/** @brief 本地日期时间 → UTC Unix（菜单"修改时间"输入用） */
uint32_t DS3231_LocalTimeToUtcUnix(const DS3231_Time *local_time)
{
    DS3231_Time tmp;
    if (!local_time) return 0;
    tmp = *local_time;
    return DS3231_GetUnix(&tmp) - (uint32_t)DS3231_LOCAL_TZ_HOURS * 3600UL;
}

/** @brief 真实芯片是否在用（1=硬件 0=软时钟回退），启动打印用 */
uint8_t DS3231_HardwarePresent(void)
{
#if DS3231_USE_SOFT
    return 0;
#else
    return s_rtc_hw_ok ? 1U : 0U;
#endif
}
