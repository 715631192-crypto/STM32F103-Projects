#ifndef __I2C_SOFT_H
#define __I2C_SOFT_H

#include "stm32f10x.h"

/*
 * 软件 I2C（GPIO 模拟）驱动
 * ------------------------------------------------
 * 引脚：PB8 = SCL，PB9 = SDA（OLED 与 DS3231 共用这条总线）
 *
 * 器件地址（8 位写地址）：
 *   SSD1306 OLED 地址： 0x78
 *   DS3231 RTC 地址： 0xD0
 */

#define IIC_SCL_H()   GPIO_SetBits(GPIOB, GPIO_Pin_8)
#define IIC_SCL_L()   GPIO_ResetBits(GPIOB, GPIO_Pin_8)
#define IIC_SDA_H()   GPIO_SetBits(GPIOB, GPIO_Pin_9)
#define IIC_SDA_L()   GPIO_ResetBits(GPIOB, GPIO_Pin_9)
#define IIC_SDA_READ()  GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_9)

void    IIC_Init(void);
uint8_t IIC_WriteBytesRaw(uint8_t dev_w, const uint8_t *data, uint16_t len);
uint8_t IIC_ReadRegs(uint8_t dev_w, uint8_t reg, uint8_t *buf, uint16_t len);
uint8_t IIC_WriteReg(uint8_t dev_w, uint8_t reg, uint8_t val);

#endif
