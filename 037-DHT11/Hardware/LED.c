#include "stm32f10x.h"
#include "LED.h"

uint8_t led_state = 0;

void LED_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_5;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStruct);

	GPIO_ResetBits(GPIOA, GPIO_Pin_5);
	led_state = 0;
}

void LED_ON(void)
{
	GPIO_SetBits(GPIOA, GPIO_Pin_5);//高电平亮
	led_state = 1;
}

void LED_OFF(void)
{
	GPIO_ResetBits(GPIOA, GPIO_Pin_5);//低电平灭
	led_state = 0;
}

void LED_Toggle(void)
{
	if (led_state)
	{
		LED_OFF();
	}
	else
	{
		LED_ON();
	}
}
