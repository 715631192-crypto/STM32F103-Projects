#ifndef __BUZZER_H_
#define __BUZZER_H_
#include "stm32f10x.h"

extern uint8_t buzzer_state;

void BUZZER_Init(void);
void BUZZER_ON(void);
void BUZZER_OFF(void);
void BUZZER_Toggle(void);

#endif // __BUZZER_H_
