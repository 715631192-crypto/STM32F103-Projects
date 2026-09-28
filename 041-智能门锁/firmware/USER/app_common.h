#ifndef __APP_COMMON_H
#define __APP_COMMON_H

#include <stdint.h>

/*
 * 应用层公共定义：认证方式、日志结果、Flash 存储布局、时间工具
 * ================================================================
 * 所有 app_* 模块共享的常量集中放在这里，避免各模块"各自约定"。
 */

/* ---------------- 认证/操作方式（日志与联动用） ---------------- */
#define AUTH_METHOD_SYSTEM   0    /* 系统动作(自动上锁等)     */
#define AUTH_METHOD_PWD      1    /* 密码开锁                 */
#define AUTH_METHOD_CARD     2    /* IC 卡开锁                */
#define AUTH_METHOD_FINGER   3    /* 指纹开锁                 */
#define AUTH_METHOD_TOTP     4    /* 动态口令开锁             */
#define AUTH_METHOD_REMOTE   5    /* 云端远程开锁             */
#define AUTH_METHOD_ADMIN    6    /* 管理员进入菜单           */

/* ---------------- 日志结果 ---------------- */
#define LOG_RESULT_OK        0    /* 成功                     */
#define LOG_RESULT_FAIL      1    /* 失败(密码错/卡无效等)    */
#define LOG_RESULT_DURESS    2    /* 胁迫开锁(静默报警)       */
#define LOG_RESULT_ALARM     3    /* 报警事件(防撬等)         */
#define LOG_RESULT_LOCK      4    /* 上锁动作                 */

/* ---------------- W25Q64 存储地址布局 ----------------
 * 8MB 空间按 4KB 扇区划分，各区域独立擦写互不影响 */
#define FLASH_ADDR_PARAMS    0x000000u  /* 系统参数(128B)     */
#define FLASH_ADDR_CARDS     0x001000u  /* 卡片表(50×8B)      */
#define FLASH_ADDR_TEMPPWD   0x002000u  /* 临时密码(4×16B)    */
#define FLASH_ADDR_LOGHDR    0x003000u  /* 日志头(条数)       */
#define FLASH_ADDR_PWDS      0x004000u  /* 密码表(6×22B)      */
#define FLASH_ADDR_LOG       0x010000u  /* 日志区(循环写入)   */
#define FLASH_END            0x800000u  /* W25Q64 总容量 8MB  */

/* ---------------- 通用工具 ---------------- */
uint32_t app_now_unix(void);   /* DS3231 当前 Unix 时间戳    */

#endif
