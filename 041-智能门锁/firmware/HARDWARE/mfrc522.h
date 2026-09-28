#ifndef __MFRC522_H
#define __MFRC522_H

#include "stm32f10x.h"

/*
 * MFRC522 射频卡读卡器驱动
 * SPI1：PA4=CS  PA5=SCK  PA6=MISO  PA7=MOSI  ·  RST=PC14（2026-09-10 起由引脚控制）
 * ------------------------------------------------------------------
 * 支持读取 MIFARE Classic 卡（S50/S70 UID 4 字节）。
 * 本项目只用"防冲撞"获取卡号(UID)，卡片即身份凭据。
 */

/* 应答状态 */
#define MI_OK      0
#define MI_NOTAGERR 1
#define MI_ERR     2
#define MAXRLEN    18

/* MIFARE 卡命令字（供上层寻卡/防冲撞用） */
#define PICC_REQIDL           0x26   /* 天线区内未进入休眠的卡 */
#define PICC_ANTICOLL1        0x93   /* 防冲撞（一级）          */

void  MFRC522_Init(void);
void  MFRC522_AntennaOn(void);
char  MFRC522_Request(uint8_t req_code, uint8_t *tag_type);
char  MFRC522_Anticoll(uint8_t *snr);          /* 读 4 字节卡号 */

/* ---- 让卡回到 IDLE（2026-09-15 新增）----
 * 读走 UID 之后卡处于 ACTIVE 态，**不再应答 REQA**：卡一直放在天线区时，
 * 下一次轮询寻不到它 —— 这就是"读一次后再也读不到、拿开再放才行"的原因。
 * 读完主动 HALT，卡立刻回 IDLE，可以马上被再次寻到（去重逻辑照旧生效）。 */
void  MFRC522_Halt(void);

/* ---- 链路自检（只读，可在启动时无条件调用）----
 * 返回 1 = SPI 双向通路正常（初始化时写入的两个寄存器都能原值读回）；
 *      0 = 读不回，SPI 完全不通（查 SCK/MOSI/MISO 线序、供电、模块好坏）
 */
uint8_t MFRC522_Check(void);

/* 读版本寄存器 VersionReg(0x37) 用于打印：
 *   真品 MFRC522 = 0x91(v1.0) / 0x92(v2.0)；国产兼容片常见 0x12 / 0x88 …
 *   0x00 或 0xFF = MISO 恒定电平 → SPI 根本没通
 */
uint8_t MFRC522_Version(void);

#endif
