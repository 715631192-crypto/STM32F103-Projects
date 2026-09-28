#ifndef __APP_LOG_H
#define __APP_LOG_H

#include <stdint.h>
#include "app_common.h"

/*
 * 黑匣子日志模块（W25Q64 循环存储，掉电不丢失）
 * ---------------------------------------------------------
 * 每条记录 16 字节：时间 + 方式 + 人员 + 结果 + 校验和
 * 写满后从头覆盖最旧记录（环形结构），共可存 ~52 万条。
 */

/* 单条日志记录（16 字节，与 Flash 中二进制布局一致） */
typedef struct
{
    uint32_t unix;      /* 事件 Unix 时间戳        */
    uint8_t  method;    /* 认证/操作方式           */
    uint8_t  user;      /* 人员: 指纹页号/卡槽/0   */
    uint8_t  result;    /* 结果码                  */
    uint8_t  rsv;       /* 保留                    */
    uint32_t sum;       /* 前 12 字节求和校验      */
    uint32_t rsv2;      /* 保留                    */
} LogRecord;

void     app_log_init(void);                          /* 从 Flash 恢复条数 */
void     app_log_add(uint8_t method, uint8_t user, uint8_t result);
uint32_t app_log_count(void);
uint8_t  app_log_read(uint32_t index, LogRecord *r);  /* 0=最新一条，失败返回0 */

#endif
