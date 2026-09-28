#ifndef SMART_LOCK_STUB_DS3231_H
#define SMART_LOCK_STUB_DS3231_H

#include <stdint.h>

/*
 * 主机侧语法检查用的 ds3231.h 垫片（2026-09-14 新增）
 * -------------------------------------------------------------------
 * 固件侧（Keil）的真头文件在 firmware/HARDWARE/ds3231.h（含 stm32f10x.h），
 * 主机语法检查无法使用；firmware/drivers/include/ds3231.h 又是另一套
 * 可移植接口（ds3231_t），与本工程应用层的用法不同。这里只垫出
 * smart_lock_app.c 管理菜单用到的**固件侧接口**，原型必须与
 * HARDWARE/ds3231.h 保持一致。
 */

#define DS3231_ADDR   0xD0
#define DS3231_USE_SOFT       0
#define DS3231_BUILD_TZ_HOURS 8
#define DS3231_LOCAL_TZ_HOURS 8

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
uint32_t DS3231_GetUnix(DS3231_Time *t);
void    DS3231_UnixToTime(uint32_t unix, DS3231_Time *t);
void     DS3231_UnixToLocalTime(uint32_t utc_unix, DS3231_Time *t);
uint32_t DS3231_LocalTimeToUtcUnix(const DS3231_Time *local_time);
uint8_t  DS3231_HardwarePresent(void);
void    DS3231_SoftSetUnix(uint32_t unix_time);
uint32_t DS3231_SoftNowUnix(void);

#endif /* SMART_LOCK_STUB_DS3231_H */
