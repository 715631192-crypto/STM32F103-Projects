#ifndef __KEY_H_
#define __KEY_H_
#include "stm32f10x.h"

extern volatile uint8_t KEY_triggered;

void KEY_Init(void);
void KEY_Scan(void);

#endif // __KEY_H_
