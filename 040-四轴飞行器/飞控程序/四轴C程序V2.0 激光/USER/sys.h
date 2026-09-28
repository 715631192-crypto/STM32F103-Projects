#ifndef __SYS_H
#define __SYS_H

//#pragma pack(push,4) 
//#pragma pack(4) 
//__unaligned
//__packed
//__align(4)

#include "delay.h"      //system delay,common.
#include "ALL_DATA.h"
#include "stm32f10x.h"
#include "ALL_DEFINE.h" 


extern void ALL_Init(void);
//extern float micros(void); //返回系统当前时间

#endif

/******************************END OF FILE *******************************************/

