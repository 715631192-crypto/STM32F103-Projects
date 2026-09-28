
#include "sys.h"

#undef SUCCESS
#define SUCCESS 0
#undef FAILED
#define FAILED  1
#define ZY_31_SLAVE_ADDRESS 0xC0
#define MODULE_REST 0x20
#define MPU6050_GET_OFFSET 0x21
#define MS5611_GET_OFFSET 0x22
#define MPU_READ 0X30
#define ANGLE_READ 0X32

#define HEIGHT_READ 0X33
#define RATE_READ 0X34

/******************************************************************/
//volatile uint32_t SysTick_count; //系统滴答时钟


/***********************************************************************
 * 
 * @param[in] 
 * @param[out] 
 * @return     
 **********************************************************************/


/**************************************************************
 * 滴答时钟ms中断
 * @param[in] 
 * @param[out] 
 * @return     
 ***************************************************************/	
//void SysTick_Handler(void)
//{
//		SysTick_count++; //ms计数
////		LED_display();  	//LED闪烁	
//}
/**************************************************************
// *返回系统当前的运行时间  单位ms
// * @param[in] 
// * @param[out] 
// * @return  ms   
// ***************************************************************/	
//float micros(void) //返回系统当前时间
//{
//	 //SysTick_count 滴答时钟ms计数
//	 //SysTick->VAL 滴答时钟定时器计数器counter 总计数满168000.0f/8.0f为1个ms，并发生滴答时钟ms中断
//	 //168MHZ/1000 = 168000每个ms需要的晶振频率输了
//	 //滴答时钟八分频
//    return SysTick_count + (SysTick->LOAD - SysTick->VAL)/(168000.0f/8.0f);//当前系统时间 单位ms  
//}
/////***********************************************************************
//// * 
//// * @param[in] 
//// * @param[out] 
// * @return     
// **********************************************************************/
//float height;

/********************************END OF FILE*****************************************/


