#ifndef __W25Q64_H
#define __W25Q64_H

#include "stm32f10x.h"

/*
 * W25Q64 SPI Flash 驱动（8MB，SPI2：PB12=CS PB13=SCK PB14=MISO PB15=MOSI）
 * -----------------------------------------------------------------------
 * 用途（黑匣子数据全部落在这里，掉电不丢失）：
 *   0x000000  4KB   系统参数区（密码哈希/配置/锁定状态）
 *   0x001000  4KB   卡片 UID 表（最多 50 张）
 *   0x002000  4KB   临时密码表（一次性/限时）
 *   0x003000  4KB   日志头（记录条数）
 *   0x010000 起     日志记录区（每条 16 字节，循环写入）
 */

#define W25Q64_JEDEC_ID   0xEF4017   /* 厂商EF + 类型40 + 容量17(8MB) */

uint8_t  W25Q64_Init(void);                 /* 返回1=检测到器件 */
uint32_t W25Q64_ReadID(void);
void     W25Q64_Read(uint32_t addr, uint8_t *buf, uint16_t len);
void     W25Q64_SectorErase(uint32_t addr); /* 4KB 扇区擦除(变0xFF)，写之前必须先擦 */
/* 页编程：可写任意长度、可跨页（驱动内部自动按 256B 页拆分）。
 * ★ 旧版要求调用者自己保证不跨页，曾导致 300 字节的卡表被回卷覆盖、掉电丢卡。 */
void     W25Q64_PageProgram(uint32_t addr, const uint8_t *buf, uint16_t len);

#endif
