#include "main.h"
//初始化beep
void beep_init(void)
{
	//蜂鸣器引脚pb5
	//使能时钟
	RCC_APB2PeriphClockCmd(BEEP_GPIO_CLK,ENABLE);
	
	//初始化并设置通用推挽输出
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.GPIO_Pin=BEEP_PIN;
	GPIO_InitStruct.GPIO_Mode=GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(BEEP_GPIO_PORT,&GPIO_InitStruct);
}
void beep_on_times(int times,int delay)
{
    for(int i=0;i<times;i++)
    {
        BEEP_ON();
        Delay_ms(delay);
        BEEP_OFF();
        Delay_ms(delay);
    }
}




