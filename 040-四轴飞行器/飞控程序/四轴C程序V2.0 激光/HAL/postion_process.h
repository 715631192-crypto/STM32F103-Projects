#ifndef _POSITION_H_
#define _POSITION_H_


//地理坐标系下
//Longitude   W_E东西经度
//Latitude    N_S南北纬度
typedef struct
{	
	float Longitude_W_E_Origin; //原始得出与HOME点的的距离
	float Latitude_N_S_Origin; 	
	float Longitude_W_E_Distance; //融合后 ，与HOME点的距离用作PID控制
	float Latitude_N_S_Distance; 
	float Longitude_W_E_Speed;    //融合后 ，当前经纬度上的速度用作PID控制
	float Latitude_N_S_Speed;
	uint8_t Filter_Defeated_Flag; //融合失败标志
}_st_Earth_Position;


extern _st_Earth_Position Earth_Position;

void Filter_Horizontal(void);
void Strapdown_INS_Horizontal(void);
#endif

