
#include "stm32f10x.h"
#include "LED.h"
#include "ALL_DATA.h"
#include "ALL_DEFINE.h" 
//---------------------------------------------------------
/*     you can select the LED statue on enum contains            */
sLED LED = {300,AllFlashLight};  //LED initial statue is off;
u8 LED_warn;                             //default 300ms flash the status

u16 falg_Time=0;

/**************************************************************
 *  LED Init
 * @param[in] 
 * @param[out] 
 * @return     
 ***************************************************************/
void LEDInit(void)	
{	
		GPIO_InitTypeDef GPIO_InitStructure;
		AFIO->MAPR = 0X02000000; //使能4线烧写 释放某些与烧写相关的引脚
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO , ENABLE);
	  GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
  	GPIO_InitStructure.GPIO_Pin = fLED_io | hLED_io|aLED_io | bLED_io;		     //LED12
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  	GPIO_Init(LED_GPIOB, &GPIO_InitStructure); 	 
	
}
/**************************************************************
 *  LED system
 * @param[in] 
 * @param[out] 
 * @return     
 ***************************************************************/
/**************************************************************
 *  LED system
 * @param[in] 
 * @param[out] 
 * @return     
 ***************************************************************/	
void PilotLED() //flash 300MS interval
{
	static uint32_t LastTime = 0;
  		
	if(SysTick_count - LastTime < LED.FlashTime)
	{

		return;
	}
	else
		LastTime = SysTick_count;
	switch(LED.status)
	{
		case AlwaysOff:      //常暗   
			bLED_H();
			fLED_H();
			bLED_H();
			hLED_H();
			break;
		case AllFlashLight:				  //全部同时闪烁
			fLED_Toggle();			
			bLED_Toggle();
			hLED_Toggle();			
			aLED_Toggle();		
		  break;
		case AlwaysOn:  //常亮
			
		  bLED_L();
			fLED_L();
			aLED_L();
			hLED_L();
		  break;
		case AlternateFlash: //交替闪烁
			bLED_H();
			fLED_L();
			aLED_H();
			hLED_L();
			LED.status = AllFlashLight;
			break;
		case WARNING:
		  fLED_Toggle();
		  hLED_Toggle();
			bLED_Toggle();
		  aLED_Toggle();
			LED.FlashTime = 800;
			break;
		case DANGEROURS:   //常亮 2秒 闪烁 1秒
			
			falg_Time++;
		  if(falg_Time>11)
			{falg_Time=0;}
			 if(falg_Time>8)
			 {
					bLED_Toggle(); //闪烁
					aLED_Toggle();
					fLED_Toggle();
					hLED_Toggle();
			 }
			 else  if((falg_Time<7))
			 {
					bLED_L();  //常亮
					fLED_L();
					aLED_L();
					hLED_L();
			 }
		 
			LED.FlashTime = 150;
			break;
		default:
			LED.status = AlwaysOff;
			break;
	}
}

/**************************END OF FILE*********************************/



