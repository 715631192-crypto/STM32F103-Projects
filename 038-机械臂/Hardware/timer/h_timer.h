
#ifndef __H_TIMER_H_
#define __H_TIMER_H_
#include "main.h"
void SysTick_Init(void);
u32 SysTick_get_ms(void);

void TIM2_init(u16 arr, u16 psc);/*初始化TIM2*/

#endif // __H_TIMER_H_



