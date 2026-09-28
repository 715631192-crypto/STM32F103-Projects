#include "./led/h_led.h"
//初始化led
void led_init(void)
{
	// PB13
	//  使能时钟
	RCC_APB2PeriphClockCmd(LED_GPIO_CLK, ENABLE);
	// GPIOA初始化，并设置通用推挽输出
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.GPIO_Pin = LED_PIN;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);
}



