#ifndef _wifi_ufo_h_
#define _wifi_ufo_h_
#include "stm32f10x.h"


extern  uint16_t WIFI_UFO_Err;
extern uint8_t WIFI_SSI,WIFI_SSI_CNT;
void WIFI_UFO_Data_Receive_Prepare(uint8_t data);
void WIFI_UFO_Data_Receive_Anl(void);
u8 WIFI_UFO_Connect(void);
static void Key_Function(u8 key);
#endif
