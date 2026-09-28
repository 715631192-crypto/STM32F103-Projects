#ifndef __KEY_H_
#define __KEY_H_
#include "main.h"
#define KEY_50MS 5
#define KEY_100MS 10
#define KEY_200MS 20
#define KEY_500MS 50
#define KEY_1S 100
#define KEY_2S 200
#define KEY_3S 300
#define DITHER_ELIMINATION_TIME KEY_50MS//消抖时间
#define LONG_PRESS_TIME KEY_2S//长按时间阈值
//定义KEY引脚
#define KEY1_PIN GPIO_Pin_8
#define KEY1_GPIO_PORT GPIOA
#define KEY1_GPIO_CLK RCC_APB2Periph_GPIOA
#define KEY2_PIN GPIO_Pin_11
#define KEY2_GPIO_PORT GPIOA
#define KEY2_GPIO_CLK RCC_APB2Periph_GPIOA
extern uint8_t key1_pressing;
extern uint8_t key2_pressing;
//控制按键
#define KEY1() GPIO_ReadInputDataBit(KEY1_GPIO_PORT, KEY1_PIN)//读取KEY1引脚状态    
#define KEY2() GPIO_ReadInputDataBit(KEY2_GPIO_PORT, KEY2_PIN)//读取KEY2引脚状态

void key_init(void);
void key_scan(void);

#endif // __KEY_H_

