#include "flow.h"
#include "myMath.h"							 
#include "spl06.h"


struct _flow_ mini;		//mini光流

void Flow_Receive1(u8 data) //串口1解析光流模块数据
{
	static u8 RxBuffer[32];//光流数据缓存
	static u8 _data_cnt = 0;//光流数据缓存计数
	static u8 state = 0; //光流数据解析状态
	u8 sum = 0;
	static u8 fault_cnt;//光流数据异常计数器
	
	
	switch(state)
	{
		case 0:
			if(data==0xFE)  //包头
			{
				state=1;
				RxBuffer[_data_cnt++]=data;
			}else state = 0;
		break;
		case 1:
			if(data==0x04)
			{
				state=2;
				RxBuffer[_data_cnt++]=data;
			}else state = 0;
		break;
		case 2:
			RxBuffer[_data_cnt++]=data;
			if(_data_cnt==11)
			{
				state = 0;
				_data_cnt = 0;
				sum =  (RxBuffer[2] + RxBuffer[3] + RxBuffer[4] + RxBuffer[5]+ RxBuffer[6] + RxBuffer[7]);
				if((0xAA == data) && (sum == RxBuffer[8])) 
				{
					Flow_SSI_CNT++;//光流数据频率
					
					//读取原始数据
					mini.flow_x = ((s16)(*(RxBuffer+3)<<8)|*(RxBuffer+2));
					mini.flow_y = ((s16)(*(RxBuffer+5)<<8)|*(RxBuffer+4));
					mini.qual = *(RxBuffer+9);//光流数据质量指标
					mini.flow_High= ((s16)(*(RxBuffer+7)<<8)|*(RxBuffer+6)); 
					
					mini.flow_x_i += mini.flow_x ;       //光流数据积分
					mini.flow_y_i += mini.flow_y ;  
					
					mini.flow_x_iOUT += mini.flow_x ;       //光流数据积分输出
					mini.flow_y_iOUT += mini.flow_y ;   
					
					//判断光流数据是否有效
					if(mini.qual<25)
					{
							fault_cnt++;													
							if(fault_cnt>60)	//连续60次异常标定为无效
							{
								fault_cnt = 60;
								mini.ok = 0;
							}
					}
					else 
					{
							fault_cnt=0;
							mini.ok = 1;
					}
				}
			}
		break;
		default:
			state = 0;
			_data_cnt = 0;
		break;
	}
}

void Flow_Receive(u8 data) //串口1解析光流模块数据
{
	static u8 RxBuffer[32];
	static u8 _data_cnt = 0;
	static u8 state = 0; 
	u8 sum = 0;
	static u8 fault_cnt;
	
	
	switch(state)
	{
		case 0:
			if(data==0xFE)  //包头
			{
				state=1;
				RxBuffer[_data_cnt++]=data;
			}else state = 0;
		break;
		case 1:
			if(data==0x04)
			{
				state=2;
				RxBuffer[_data_cnt++]=data;
			}else state = 0;
		break;
		case 2:
			RxBuffer[_data_cnt++]=data;
			if(_data_cnt==9)
			{
				state = 0;
				_data_cnt = 0;
				sum =  (RxBuffer[2] + RxBuffer[3] + RxBuffer[4] + RxBuffer[5]);
				if((0xAA == data) && (sum == RxBuffer[6])) 
				{
					Flow_SSI_CNT++;//光流数据频率
					
					//读取原始数据
					mini.flow_x = ((s16)(*(RxBuffer+3)<<8)|*(RxBuffer+2));
					mini.flow_y = ((s16)(*(RxBuffer+5)<<8)|*(RxBuffer+4));
					mini.qual = *(RxBuffer+7);
					
					mini.flow_x_i += mini.flow_x ;
					mini.flow_y_i += mini.flow_y ;
					
					//判断光流数据是否有效
					if(mini.qual<25)
					{
							fault_cnt++;													
							if(fault_cnt>60)	//连续60次异常标定为无效
							{
								fault_cnt = 60;
								mini.ok = 0;
							}
					}
					else 
					{
							fault_cnt=0;
							mini.ok = 1;
					}
				}
			}
		break;
		default:
			state = 0;
			_data_cnt = 0;
		break;
	}
}


struct _pixel_flow_ pixel_flow;//光流输出数据结构体


//mini光流数据滤波融合
void Pixel_Flow_Fix(float dT)
{
//	float high_temp;
	float cpi;
	
	//如果光流模块异常直接退出
	if(Flow_Err ==1 || !mini.ok)
	{		
		mini.flow_x_i=0;
		mini.flow_y_i=0;
		pixel_flow.fix_x = 0;
		pixel_flow.fix_y = 0;	
		return;
	}
	
	////////////////////////*积分位移处理*////////////////////////////
	//低通滤波
	pixel_flow.fix_x_i += ((mini.flow_x_i - pixel_flow.fix_x_i) *0.2);
	pixel_flow.fix_y_i += ((mini.flow_y_i - pixel_flow.fix_y_i) *0.2);
	
	//传感器倾角参数  用姿态角去补偿积分位移（#define  angle_to_rad  0.0174f  //角度转弧度）
	pixel_flow.ang_x += ( 600.0f*tan(-Angle.pitch*ANG_2_RAD) - pixel_flow.ang_x) *0.2;
	pixel_flow.ang_y += ( 600.0f*tan(-Angle.roll*ANG_2_RAD) - pixel_flow.ang_y) *0.2;

	 
	//位移与角度互补融合
	pixel_flow.out_x_i = pixel_flow.fix_x_i - pixel_flow.ang_x;  
	pixel_flow.out_y_i = pixel_flow.fix_y_i - pixel_flow.ang_y;
	

	
	////////////////////////*微分位移处理*////////////////////////////
	
	//对积分位移进行微分处理，得到速度。
	
	//求微分速度
	pixel_flow.x = (pixel_flow.out_x_i - pixel_flow.out_x_i_o)/dT;	
	pixel_flow.out_x_i_o = pixel_flow.out_x_i;
	pixel_flow.y = (pixel_flow.out_y_i - pixel_flow.out_y_i_o)/dT;	
	pixel_flow.out_y_i_o = pixel_flow.out_y_i;
	
	//低通滤波
	pixel_flow.fix_x += ( pixel_flow.x - pixel_flow.fix_x ) * 0.1f;
	pixel_flow.fix_y += ( pixel_flow.y - pixel_flow.fix_y ) * 0.1f;
	
	
		///////////////////*光流数据与高度数据融合*//////////////////////////
		
	//式中HIGH为实际高度，单位：米
//	 cpi = ((FlightData.High.bara_height*0.01f) / 11.914f) *2.54f ;
	 cpi = ((50*0.01f) / 11.914f) *2.54f ;//固定高度方便调试
	 pixel_flow.fix_High=cpi;
	 
	//积分位移值单位转换为：厘米
	pixel_flow.loc_x = pixel_flow.out_x_i * cpi;
	pixel_flow.loc_y = pixel_flow.out_y_i * cpi;
	 
	//微分速度值单位转换为：厘米/秒
	pixel_flow.loc_xs = pixel_flow.fix_x * cpi; 
	pixel_flow.loc_ys = pixel_flow.fix_y * cpi;
	
	

///////////////////////*光流数据与高度数据融合*////////////////////////////////////////////////////
//// high_temp = (wcz_h_fus.out - high_begin);
//	
////	high_temp = 50;//固定值方便调试
//	
//	high_temp = FlightData.High.bara_height; //实时高度值
//	
//	cpi = 11.914f / LIMIT(high_temp*0.01f,0.1f,1.5f) ;
//	
//	pixel_flow.dx = (pixel_flow.out_x_i - pixel_flow.out_x_i_o)/cpi*2.54f; 
//	pixel_flow.out_x_i_o = pixel_flow.out_x_i;
//	pixel_flow.dy = (pixel_flow.out_y_i - pixel_flow.out_y_i_o)/cpi*2.54f; 
//	pixel_flow.out_y_i_o = pixel_flow.out_y_i;


//	////////////////////////*微分数据处理*///////////////////////////////

//	//微分求速度(单位：cm/s)
//	pixel_flow.x = pixel_flow.dx/dT;
//	pixel_flow.y = pixel_flow.dy/dT;
//	
//	//低通滤波
//	pixel_flow.fix_x += ( pixel_flow.x - pixel_flow.fix_x ) * 0.1f;
//	pixel_flow.fix_y += ( pixel_flow.y - pixel_flow.fix_y ) * 0.1f;
//	
//	//累加求位移(不需要乘以周期dt，单位：cm)
//	pixel_flow.x_i += pixel_flow.dx;
//	pixel_flow.y_i += pixel_flow.dy;
}
