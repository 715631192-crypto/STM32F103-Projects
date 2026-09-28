#ifndef SMART_LOCK_STUB_AS608_H
#define SMART_LOCK_STUB_AS608_H

#include <stdint.h>

/*
 * 主机侧语法检查用的 as608.h 垫片（2026-09-14 新增）
 * -------------------------------------------------------------------
 * smart_lock_app.c 的管理菜单要直接调 AS608 的录入/删除原语。
 * 固件侧（Keil）从 firmware/HARDWARE/as608.h 取真声明；但那份头文件
 * include 了 stm32f10x.h，主机语法检查（GCC + tests/stubs）没法用。
 * 本垫片只保留**完全相同**的函数原型与宏，主机上即可编译。
 * 两侧原型若改动，必须同步修改，否则主机检查会失真。
 */

#define AS608_USER_PAGE_MAX   48
#define AS608_DURESS_PAGE     49
#define AS608_ERR_TIMEOUT   (-1)

int      AS608_Check(void);
int      AS608_GetImage(void);
int      AS608_GenChar(uint8_t bufid);
int      AS608_RegModel(void);
int      AS608_Search(uint8_t bufid, uint16_t start_page, uint16_t count,
                      uint16_t *page_id, uint16_t *score);
int      AS608_StoreChar(uint8_t bufid, uint16_t page_id);
int      AS608_DeleteChar(uint16_t page_id, uint16_t count);
int      AS608_Empty(void);
int      AS608_Identify(uint16_t *page_id, uint16_t *score);

#endif /* SMART_LOCK_STUB_AS608_H */
