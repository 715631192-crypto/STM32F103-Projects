#ifndef __H_BEEP_H_
#define __H_BEEP_H_
#include "main.h"
//定义LED引脚
#define BEEP_GPIO_PORT GPIOB
#define BEEP_PIN GPIO_Pin_5
#define BEEP_GPIO_CLK RCC_APB2Periph_GPIOB

//控制蜂鸣器的宏
#define BEEP_GET_LEVEL() GPIO_ReadOutputDataBit(BEEP_GPIO_PORT, BEEP_PIN) //获取蜂鸣器状态
#define BEEP_ON() GPIO_SetBits(BEEP_GPIO_PORT, BEEP_PIN) //蜂鸣器响
#define BEEP_OFF() GPIO_ResetBits(BEEP_GPIO_PORT, BEEP_PIN) //蜂鸣器静音
#define BEEP_TOGGLE() GPIO_WriteBit(BEEP_GPIO_PORT, BEEP_PIN, (BitAction)(1-BEEP_GET_LEVEL()))//翻转蜂鸣器状态

void beep_init(void);
void beep_on_times(int times,int delay);
#endif // __H_BEEP_H_

