#ifndef __LED_H_
#define __LED_H_
#include "stm32f10x.h"

extern uint8_t led_state;

void LED_Init(void);
void LED_ON(void);
void LED_OFF(void);
void LED_Toggle(void);

#endif // __LED_H_
