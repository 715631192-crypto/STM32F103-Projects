
#include "stm32f10x.h"
#include "misc.h"
#include "delay.h"
#include "ALL_DATA.h"
#include "scheduler.h"
static volatile uint32_t usTicks = 0;

volatile uint32_t SysTick_count22; //系统滴答时钟
u8 sys_init_ok = 1;
//// ????????? ,49????
//volatile uint32_t sysTickUptime = 0;
void cycleCounterInit(void)
{
    RCC_ClocksTypeDef clocks;
    RCC_GetClocksFreq(&clocks);
    usTicks = clocks.SYSCLK_Frequency / 1000000;
}
void SysTick_IRQ(void)//1ms??
{
	static u8 cnt;
	
		SysTick_count++;
	if(!sys_init_ok) return;
	
	//LED_1ms_DRV();//?????
	
	cnt++;	cnt %= 2;
	if(cnt)	Loop_check();//2ms???? 
}  

uint32_t GetSysTime_us(void) 
{
    register uint32_t ms, cycle_cnt;
    do {
        ms = SysTick_count;
        cycle_cnt = SysTick->VAL;
    	} while (ms != SysTick_count);
    return (ms * 1000) + (usTicks * 1000 - cycle_cnt) / usTicks;
}

//    毫秒级延时函数	 
void delay_ms(uint16_t nms)
{
	uint32_t t0=GetSysTime_us();
	while(GetSysTime_us() - t0 < nms * 1000);	
	SysTick_count22++;
}

void delay_us(unsigned int i)
 {  
	char x=0;   
    while( i--)
    {	
       for(x=1;x>0;x--);
    }
 }	

/**************************************************************
 *返回系统当前的运行时间  单位ms
 * @param[in] 
 * @param[out] 
 * @return  ms   
 ***************************************************************/	
float micros(void) //返回系统当前时间
{
	 //SysTick_count 滴答时钟ms计数
	 //SysTick->VAL 滴答时钟定时器计数器counter 总计数满168000.0f/8.0f为1个ms，并发生滴答时钟ms中断
	 //168MHZ/1000 = 168000每个ms需要的晶振频率输了
	 //滴答时钟八分频
    return SysTick_count22 + (SysTick->LOAD - SysTick->VAL)/(72000.0f/8.0f);//当前系统时间 单位ms  
}
///***********************************************************************
// * 
// * @param[in] 
// * @param[out] 
// * @return     
// **********************************************************************/
//float height;

/********************************END OF FILE*****************************************/ 
