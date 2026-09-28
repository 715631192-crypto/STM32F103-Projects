#ifndef __OLED_H
#define __OLED_H

#include "stm32f10x.h"

/*
 * 0.96 寸 SSD1306 OLED 驱动（I2C 接口，地址 0x78）
 * ------------------------------------------------
 * 分辨率 128x64，按"页"组织：共 8 页，每页 8 行像素。
 * 说明：本驱动不建显存缓冲（直接写屏），节省 1KB RAM。
 *
 * 两套字库 / 两套 API：
 *   ① 6x8 小字（oledfont.h）
 *        OLED_ShowChar / OLED_ShowString / OLED_ShowNum / OLED_ShowTwoNum
 *        y 取值 0~7（页号），一行可放 8 行字
 *   ② 16 像素行高「中文」字库（oledfont_cn.h，汉字 16x16 + ASCII 8x16）
 *        OLED_ShowText / OLED_TextWidth
 *        y 只取 0/2/4/6（占 2 页），整屏 4 行；每行最多 8 个汉字
 *        字符宽度：汉字 16px，ASCII 8px
 *  中文界面统一走 ②。OLED_ShowText 的字符串是 UTF-8，可中英混排。
 */

#define OLED_ADDR      0x78     /* SSD1306 8位写地址 */
#define OLED_CMD       0x00     /* 控制字节：命令     */
#define OLED_DATA      0x40     /* 控制字节：数据     */

void OLED_Init(void);
void OLED_DisplayOn(void);
void OLED_DisplayOff(void);
void OLED_Clear(void);
void OLED_SetPos(uint8_t x, uint8_t y);

/* ---- 6x8 小字（y = 页号 0~7） ---- */
void OLED_ShowChar(uint8_t x, uint8_t y, char ch);
void OLED_ShowString(uint8_t x, uint8_t y, const char *str);
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len);
void OLED_ShowTwoNum(uint8_t x, uint8_t y, uint8_t num);   /* 两位显示 0x~ */

/* ---- 16 像素行高中文（y = 0/2/4/6，占 2 页） ---- */
void    OLED_ShowText(uint8_t x, uint8_t y, const char *utf8);  /* UTF-8，可中英混排 */
uint8_t OLED_TextWidth(const char *utf8);                       /* 该串像素宽度，用来居中 */

#endif
