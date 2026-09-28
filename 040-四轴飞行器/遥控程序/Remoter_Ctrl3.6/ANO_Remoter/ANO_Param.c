#include "ANO_Param.h"
#include "ANO_Stick.h"
#include "ANO_Drv_MPU6050.h"

#define FIRSTINITFLAH  			0x44

struct param ANO_Param;

void ANO_Param_Init(void)//恢复默认参数
{
	ANO_Param.NRF_Channel= 0;
	ANO_Param.OffSet_En  = 0;
	ANO_Param.OffSet_Rol = 0;
	ANO_Param.OffSet_Pit = 0;
	ANO_Param.OffSet_Yaw = 0;
	ANO_Param.OffSet_Thr = 0;
	ANO_Param.FirstInitFlag = FIRSTINITFLAH;
	
	ANO_Param_SAVE();						//存储数据
}

void ANO_Param_SAVE(void)   //存储数据
{
	ANO_Flash_Write((u8 *)(&ANO_Param),sizeof(ANO_Param));  //写进Flash
}

void ANO_Param_READ(void)
{
		
	ANO_Flash_Read((u8 *)(&ANO_Param),sizeof(ANO_Param));   //上电读出存储数据
	if(ANO_Param.FirstInitFlag != FIRSTINITFLAH)//板子从未初始化
	{
		ANO_Param_Init();  //复位数据
	}
}










