#include "sysconfig.h"

uint8_t Mode=1,Fun=0;//默认锁尾模式、非夜间模式
/******************************************************************************
函数原型:	void KEY_Init(void)
功　　能:	按键初始化
*******************************************************************************/ 
void KEY_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO,ENABLE);
	
		//改变指定管脚的映射 GPIO_Remap_SWJ_Disable SWJ 完全禁用（JTAG+SW-DP）
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);
	//改变指定管脚的映射 GPIO_Remap_SWJ_JTAGDisable ，JTAG-DP 禁用 + SW-DP 使能
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable , ENABLE);
	
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_8|GPIO_Pin_11;    
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;    //上拉输入
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	_offset.pitch=_offset.roll=2048;  //微调按键值L R
	_offset.thr=2048; //微调按键值M
	
}


void key_function(void)  //前面两个左右按键的程序
{
	static u8 count0=0,count1=0,beet_falg=0,beet_temp;
	
	Rc.AUX4 = 0;
	if(!Mode_Read)	Rc.AUX4 |= 0x01;//左边按键
	if(!Fun_Read)		Rc.AUX4 |= 0x02;//右边按键
	
	///////////////////////////////////////////////////////
	if(Rc.AUX4&0x01)   //右边按键
	{
		count0++;
		if(count0==200)   //长按2s遥控器搜索飞机 对频
		{
			Check_Ch = 1;   //对频变量
			ANO_LED_0_FLASH();		//led闪烁
		}
		if(count0>=250)	count0=250;
	}
	else
	{
		if( count0>=2 && count0<=100 ) //短按 
		{
			if(Show.hardware_type==1)
			{
				beet_falg=1;
				Show.windows++;
				if(Show.windows>2 || Show.set_flag&BIT6) Show.windows=0;
			}
		}
		count0=0;
	}
	///////////////////////////////////////////////////////		
	if(Rc.AUX4&0x02)//左边按键
	{
		count1++;
		if(count1==200)//长按2s，摇杆中位校准
		{
			ANO_Param.OffSet_En = 1;
			ANO_LED_0_FLASH();
		}
		if(count1>=250)	count1=250;
	}
	else
	{
		if( count1>=2 && count1<=100 ) //短按 定高变量
		{
				Rc.AUX2 ^= (2000^1000);
			  beet_falg=1;
		}
		count1=0;
	}
	
	if(beet_falg==1) //控制蜂鸣器响
	{
			BEEP_L; //开蜂鸣器
		  beet_temp++;
			if(beet_temp>5)
			{
					beet_temp=0;
				  beet_falg=0;
					BEEP_H;  //关蜂鸣器
			}
	
	}
}


/******************************************************************************
微调4个按键程序
*******************************************************************************/ 
void ANO_key(void)
{
	#define KEY3 GPIO_Pin_3			//GPIO
	#define KEY4 GPIO_Pin_4			//GPIO
	#define KEY5 GPIO_Pin_5 		//GPIO
	#define KEY6 GPIO_Pin_6 		//GPIO
	#define KEY2 GPIO_Pin_2 		//GPIO
	#define KEY11 GPIO_Pin_11 	//GPIO
  volatile static uint8_t status = 0;	
	volatile static uint8_t BEET_flag=0;
 	static uint32_t temp,BUT_mun;
		switch(status)
		{
				case 0:
					if(SysTick_count - temp >10) //
					{
						if(
							((GPIOB->IDR & (GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_2|GPIO_Pin_11)) == (GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_2|GPIO_Pin_11))					
							)
							status = 1;
						BEEP_H;
						LED_Red_OFF;//?LED
					}
					break;
				case 1:
						if(
							((GPIOB->IDR & (GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_2|GPIO_Pin_11)) != (GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_2|GPIO_Pin_11))					
							)
						status = 2;
					break;
				case 2:	
						if(!(GPIOB->IDR & KEY6))   
					{
							ANO_Param.OffSet_Rol=ANO_Param.OffSet_Rol-10;
		//					Rc.AUX1 = 2000 - (uint16_t)(0.25f*_offset.roll);	
						if(ANO_Param.OffSet_Rol == ANO_Param.Z_OffSet_Rol)      //微调回原始值判断
						{	BEET_flag=1;  }				
					}
					else if(!(GPIOB->IDR & KEY4)) 
					{
							ANO_Param.OffSet_Rol=ANO_Param.OffSet_Rol+10;
		//					Rc.AUX1 ^= 2000 - (uint16_t)(0.25f*_offset.roll);		
							if(ANO_Param.OffSet_Rol == ANO_Param.Z_OffSet_Rol)    //微调回原始值判断
						  {	BEET_flag=1;  }	
					}
					else if(!(GPIOB->IDR & KEY5))
					{
							ANO_Param.OffSet_Pit=ANO_Param.OffSet_Pit+10;
							if(ANO_Param.OffSet_Pit == ANO_Param.Z_OffSet_Pit)    //微调回原始值判断
						  {	BEET_flag=1;  }	
					}
					else if(!(GPIOB->IDR & KEY3))
					{
							ANO_Param.OffSet_Pit=ANO_Param.OffSet_Pit-10;
							if(ANO_Param.OffSet_Pit == ANO_Param.Z_OffSet_Pit)   //微调回原始值判断
						  {	BEET_flag=1;  }
					}
					else if(!(GPIOB->IDR & KEY2))
					{
							ANO_Param.OffSet_Yaw=ANO_Param.OffSet_Yaw+10; 
						  if(ANO_Param.OffSet_Yaw == ANO_Param.Z_OffSet_Yaw)   //微调回原始值判断
						  {	BEET_flag=1;  }
					}
					else if(!(GPIOB->IDR & KEY11))
					{
							ANO_Param.OffSet_Yaw=ANO_Param.OffSet_Yaw-10; //
							if(ANO_Param.OffSet_Yaw == ANO_Param.Z_OffSet_Yaw)   //微调回原始值判断
						  {	BEET_flag=1;  }
					}	
					status = 0;			
					BEEP_L;  
					LED_Red_On;  //开LED
					temp = SysTick_count;
					break;
		}
		
		if(BEET_flag!=0)	//微调回原始值就响2声蜂鸣器
		{
				   
			    BUT_mun++;								     //声音延时用
					if(BUT_mun==1||BUT_mun==20||BUT_mun==40)   //延时判断
					{																								
							BEEP_L;
							LED_Red_On;//开LED
																			
					}
					else if((BUT_mun>10 && BUT_mun<20) || (BUT_mun>30 && BUT_mun<40) || (BUT_mun>50 && BUT_mun<61))  //延时判断
					{
							BEEP_H;  						  
							LED_Red_OFF;  //关LED
						 if(BUT_mun==60){BEET_flag=0;BUT_mun=0;}
					}
				 
		}
		
}