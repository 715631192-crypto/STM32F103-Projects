#include "stm32f10x.h"
#include "BUZZER.h"

uint8_t buzzer_state = 0;

void BUZZER_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStruct);

	GPIO_SetBits(GPIOA, GPIO_Pin_6);
	buzzer_state = 0;
}

void BUZZER_OFF(void)
{
	GPIO_SetBits(GPIOA, GPIO_Pin_6);
	buzzer_state = 1;
}

void BUZZER_ON(void)
{
	GPIO_ResetBits(GPIOA, GPIO_Pin_6);
	buzzer_state = 0;
}

void BUZZER_Toggle(void)
{
	if (buzzer_state)
	{
		BUZZER_ON();
	}
	else
	{
		BUZZER_OFF();
	}
}
