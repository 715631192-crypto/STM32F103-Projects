#ifndef __H_PS2_H_
#define __H_PS2_H_
#include "main.h"
extern u8 psx_buf[9];
extern u8 ps2_isConnected;

//定义PS2引脚
#define PS2_DAT_PIN GPIO_Pin_15//SOMI
#define PS2_DAT_GPIO_PORT GPIOA
#define PS2_DAT_GPIO_CLK RCC_APB2Periph_GPIOA
#define PS2_CMD_PIN GPIO_Pin_14//MOSI
#define PS2_CMD_GPIO_PORT GPIOA
#define PS2_CMD_GPIO_CLK RCC_APB2Periph_GPIOA
#define PS2_CS_PIN GPIO_Pin_13
#define PS2_CS_GPIO_PORT GPIOA
#define PS2_CS_GPIO_CLK RCC_APB2Periph_GPIOA
#define PS2_CLK_PIN GPIO_Pin_12
#define PS2_CLK_GPIO_PORT GPIOA
#define PS2_CLK_GPIO_CLK RCC_APB2Periph_GPIOA

//PS2相关指令
#define START_CMD 0x01
#define ASK_DAT_CMD 0x42

//PS2模式数据表
#define PS2_MODE_GRN 0x41//模拟绿灯
#define PS2_MODE_RED 0x73//模拟红灯

//控制ps2的宏
#define PS2_DAT() GPIO_ReadInputDataBit(PS2_DAT_GPIO_PORT,PS2_DAT_PIN)//读取输入信号
#define PS2_CMD(x) GPIO_WriteBit(PS2_CMD_GPIO_PORT,PS2_CMD_PIN,(BitAction)(x))//写入输出信号
#define PS2_CLK(x) GPIO_WriteBit(PS2_CLK_GPIO_PORT,PS2_CLK_PIN,(BitAction)(x))//片选信号
#define PS2_CS(x) GPIO_WriteBit(PS2_CS_GPIO_PORT,PS2_CS_PIN,(BitAction)(x))//时钟信号

//函数声明
void ps2_init(void);
void ps2_write_read(void);
u8 ps2_transfer(unsigned char dat);

#endif // __H_PS2_H_