/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"
#include "sysconfig.h"
#include "ANO_Param.h"
#include "ANO_Drv_Flash.h"
uint8_t 	NRF_ENABLE = 0;

//配置时钟
void RCC_Configuration(void)
{
		
  
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA 
                           |RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC
						   |RCC_APB2Periph_ADC1  | RCC_APB2Periph_AFIO 
                           |RCC_APB2Periph_SPI1  
						   	, ENABLE );
	
   	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3 | RCC_APB1Periph_I2C1| RCC_APB1Periph_TIM3, ENABLE );
	
}
int main(void)
{
	
	RCC_Configuration();	//配置时钟,开时钟 USART1/SPI1/ADC1/TIM3/USART3/I2C1
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);		//中断优先级组别设置
	ANO_LED_Init();     //初始化LED	+ 蜂鸣器
	ANO_Param_READ();   //参数初始化,从 Flash 读参数			
	USB_HID_Init();	    //初始化USB	,用于遥控器与电脑通信
	ANO_SPI_Init();	    //初始化NRF所用SPI	
	
	NRF_ENABLE = ANO_NRF_Check();//检查NRF连接是否正常	
	
	if(NRF_ENABLE)     //NRF24L01模块如果连接正常，则将NRF初始化为主发送模式
	{
		ANO_NRF_Init(MODEL_TX2,ANO_Param.NRF_Channel);  ////设置频率信道
		Show.NRF_Err = 0;
	}else Show.NRF_Err = 1;
		
	ANO_LED_0_FLASH();//LED闪烁	 	
	ADC1_Init();  		//初始化ADC采样	
	KEY_Init();				//按键初始化	
	OLED_Init();			//初始化屏幕
	
	SysTick_Config(SystemCoreClock / 500);  //初始化系统滴答定时器,2ms中断
	
	while (1)                              //  注意！！！  全部运行程序在 ANO_Scheduler.c 程序里
	{																			 //  注意！！！  全部运行程序在 ANO_Scheduler.c 程序里
		if(Show.oled_delay)									 //  注意！！！  全部运行程序在 ANO_Scheduler.c 程序里
		{
			Show_Duty();  //显示程序
			Show.oled_delay=0;
		}
	}
}
