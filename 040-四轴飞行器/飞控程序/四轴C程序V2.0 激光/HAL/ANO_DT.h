#ifndef _DATA_TRANSFER_H
#define	_DATA_TRANSFER_H


#include "ALL_DEFINE.h"

#define BIT0	0x0001
#define BIT1	0x0002
#define BIT2	0x0004
#define BIT3	0x0008
#define BIT4	0x0010
#define BIT5	0x0020
#define BIT6	0x0040
#define BIT7	0x0080
#define BIT8	0x0100
#define BIT9	0x0200
#define BIT10 0x0400
#define BIT11 0x0800

#define BIT15 0x8000

enum ANTO_SEND{
	ANTO_VER      = 0x00,
	ANTO_STATUS    = 0x01,
	ANTO_MPU_MAGIC   = 0X02,
	ANTO_RCDATA   = 0x03,
	ANTO_GPSDATA  = 0x04,
	ANTO_POWER    = 0x05,
	ANTO_MOTOR    = 0x06,
	ANTO_SENSER2  = 0x07,	
	ANTO_RESERD1  = 0x08,
	ANTO_RESERD2  = 0x09,	
	ANTO_FLY_MODE = 0x0A,	
	ANTO_RATE_PID       = 0x10,	
	ANTO_ANGLE_PID      = 0x11,
	ANTO_HEIGHT_PID  = 0x12,		
	ANTO_PID4   = 0x13,	
	ANTO_PID5    = 0x14,	
	ANTO_PID6   = 0x15, 	
	ANTO_CHECK	 = 0xEF
};


typedef struct
{
	u8 thr_low;
	u8 NS; //信号来源，0：无信号
	u8 signal_loss;
	u8 low_power;
	u8 landed;
	u8 espDownloadMode;	
}_flag_st;

extern _flag_st flag;

extern void ANO_Recive(int8_t *pt);
extern void ANTO_polling(void);
void ANO_DT_Data_Receive_Prepare(u8 data);
void ANO_DT_Data_Receive_Anl(u8 *data_buf,u8 num);

#endif







