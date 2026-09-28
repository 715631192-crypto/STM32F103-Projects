
#include "WIFI_UFO.h"
#include "Remote.h"
#include "ANO_DT.h"
#include "ALL_DATA.h"
#include "myMath.h"


////////////////////////////////////////////////////
//以下是WIFI_UFO通信协议代码
////////////////////////////////////////////////////
static uint8_t wifi_RxBuffer[8];
uint16_t WIFI_UFO_Err;
uint8_t WIFI_SSI,WIFI_SSI_CNT;//WIFI信号

static uint8_t wifi_PUI=0;

//WIFI_UFO数据接收函数
void WIFI_UFO_Data_Receive_Prepare(uint8_t data)
{
	static uint8_t _data_cnt = 0;
	static uint8_t state = 0;
//	ANO_Uart1_Put_Char(data);  //串口1输出
	
	switch(state)
	{
		case 0:
			if(data==0x66)  //包头
			{
				state=1;
				wifi_RxBuffer[_data_cnt++]=data;
			}else state = 0;
		break;
		case 1:
			wifi_RxBuffer[_data_cnt++]=data;
			if(_data_cnt==7 && wifi_RxBuffer[7]==0x99)
			{
					state = 0;
					_data_cnt = 0;
					if( wifi_RxBuffer[7] == 0x99){ WIFI_UFO_Err=0;		}			//接收标志位清0	
					
					//读取数据
					Remote.thr = ((float)(wifi_RxBuffer[3])/256) * 1000 + 1000;    //油门
					LIMIT(Remote.thr,1000,2000);
					Remote.yaw  = ((float)(wifi_RxBuffer[4])*3.90625) + 1000;      //航向
					LIMIT(Remote.yaw,1000,2000);
					Remote.roll = (((float)(wifi_RxBuffer[1])/256)*1000) + 1000;   //左右
					LIMIT(Remote.roll,1000,2000);
					Remote.pitch = (((float)(wifi_RxBuffer[2])/256)*1000)+ 1000;   //前后 
					LIMIT(Remote.pitch,1000,2000);
						
					//////////////////////////////////////////////////////
					//标志位
					Remote.AUX2 = (wifi_RxBuffer[5]);
					LIMIT(Remote.AUX2,0,128);											
			}
		break;
		default:
			state = 0;
			_data_cnt = 0;
		break;
	}
	
}


//WIFI_UFO数据处理函数   66 80 80 00 80 00 80 99
void WIFI_UFO_Data_Receive_Anl(void)   
{
	static u8 temp;
	static u8 count10=0;
  flag.NS = 2;//遥控数据来源（1：遥控器。2：WiFi图传模块。3：蓝牙模块）  

	if(temp != Remote.AUX2)//标志位判断
	{
			temp = Remote.AUX2;
			switch(temp)
			{
				case 1://一键起飞  手机打开定高模式才有效
							wifi_PUI = 1;
					break;
				case 2://一键降落  手机打开定高模式才有效
							wifi_PUI = 1;
					break;
				case 4://紧急停机   设置按键按下一下接3次128
							ALL_flag.unlock = 0; 				//退出控制
							LED.status = AllFlashLight; //开始闪灯
							wifi_PUI = 1;
					break;
				case 8://3D翻滚   按一次 要控制方向才输出8 
							wifi_PUI = 1;
					break;
				case 16: //无头模式 按一次连续16 在按就是0
							wifi_PUI = 1;
					break;
				case 128://设置按键  按下一下接3次128
							wifi_PUI = 1;
					break;
				default:	
							wifi_PUI = 0;
				break;
			}		
	}
	
	  count10++;
		if(count10>=2)
		{
			count10=0;		
			if(wifi_PUI==1)
			{	
//				printf("%d  \r\n",Remote.AUX2);  //串口1输出
			}
//			printf("   %d   %d   %d   %d   %d   %d     \r\n",wifi_RxBuffer[1],wifi_RxBuffer[2],wifi_RxBuffer[3],wifi_RxBuffer[4],wifi_RxBuffer[5],wifi_RxBuffer[6]);  //串口1输出
			
			printf("   %d   %d   %d   %d   %d        \r\n",Remote.thr,Remote.yaw,Remote.roll,Remote.pitch,Remote.AUX2);  //串口1输出
		}
				
}

//WIFI_UFO连接函数
 u8 WIFI_UFO_Connect(void)
{
	static u8 Connect_flag;
	WIFI_UFO_Err ++;
	if(WIFI_UFO_Err==1)   //在接收程序里清零
	{
		WIFI_UFO_Data_Receive_Anl();
		WIFI_SSI_CNT++;
		Connect_flag = 1;
		
	}
	if(WIFI_UFO_Err>=30)
	{
		WIFI_UFO_Err = 1;
		Connect_flag = 0;
		flag.NS = 0;
//		printf("wifi漏包 \r\n");  //串口1输出	
	}
	return Connect_flag;
}
