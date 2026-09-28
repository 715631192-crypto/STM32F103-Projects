#ifndef __H_LED_H_
#define __H_LED_H_
#include "main.h"
//定义LED引脚
#define LED_GPIO_PORT GPIOB
#define LED_PIN GPIO_Pin_13
#define LED_GPIO_CLK RCC_APB2Periph_GPIOB

//控制LED的宏
#define LED_GET_LEVEL() GPIO_ReadOutputDataBit(LED_GPIO_PORT, LED_PIN) //获取LED状态
#define LED_ON() GPIO_ResetBits(LED_GPIO_PORT, LED_PIN) //LED亮
#define LED_OFF() GPIO_SetBits(LED_GPIO_PORT, LED_PIN) //LED灭
#define LED_TOGGLE() GPIO_WriteBit(LED_GPIO_PORT, LED_PIN,(BitAction)(1-LED_GET_LEVEL())) //翻转LED状态
void led_init(void);
#endif // __H_LED_H_

