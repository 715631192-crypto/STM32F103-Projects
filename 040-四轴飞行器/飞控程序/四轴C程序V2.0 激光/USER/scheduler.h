#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "stm32f10x.h"

typedef struct
{
	u8 check_flag;
	u16 err_flag;
	s16 cnt_2ms;
	s16 cnt_4ms;
	s16 cnt_6ms;
	s16 cnt_10ms;
	s16 cnt_20ms;
	s16 cnt_50ms;
	s16 cnt_1000ms;
}loop_t;

void main_loop(void);
void Loop_check(void);
void Duty_2ms();
void Duty_4ms();
void Duty_6ms();
void Duty_10ms();
void Duty_20ms();
void Duty_50ms();
void Duty_1000ms();
#endif