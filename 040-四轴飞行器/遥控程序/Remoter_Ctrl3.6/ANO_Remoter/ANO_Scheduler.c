#include "ANO_Scheduler.h"
#include "sysconfig.h"

uint16_t S_cnt_2ms=0,S_cnt_4ms=0,S_cnt_10ms=0,S_cnt_20ms=0,S_cnt_50ms=0,S_cnt_1000ms=0;

void Loop_check(void)
{
	S_cnt_2ms++;
	S_cnt_4ms++;
	S_cnt_10ms++;
	S_cnt_20ms++;
	S_cnt_50ms++;
	S_cnt_1000ms++;
	
	ANO_Loop();
}

static void ANO_Loop_500Hz(void)	//2ms执行一次
{	
	ANO_NRF_Check_Event();//检查是否收到飞机数据
	if(NRF_Evnet)//如果收到数据就把它发到上位机
	{
		Usb_Hid_Adddata(NRF24L01_2_RXDATA , RX_LEN);
		Usb_Hid_Send();   //数据发到上位机
		NRF_Evnet = 0;
	}
	ANO_Stick_Scan();  //读取遥杆等数据
}

static void ANO_Loop_250Hz(void)	//4ms执行一次
{
	
	if (USB_ReceiveFlg == TRUE)  //如果收到上位机数据就发送到飞机，否则发送遥控数据
	{
		if(Hid_RxData[0] < 33)
			ANO_NRF_TxPacket(&(Hid_RxData[1]),Hid_RxData[0]);//组帧 AA AF 03 + 通道值 + 校验和，经 ANO_NRF_TxPacket() 发往飞机。

		USB_ReceiveFlg = 0x00;	
	}
	else
	{
		ANO_DT_Send_RCData();  //发送控制数据给飞机
	}
}

static void ANO_Loop_100Hz(void)	//10ms执行一次
{
	
	
	key_function();			//按键检测
	Gesture_Check();		//显示屏摇杆上下选择操作检测
	SysTick_count++; 		 //按键扫描延时用
	
	if(!Show.Connect_Succeed)//如果与飞机连接失败就发送遥控数据到电脑
	{
		ANO_DT_Send_RCData_To_Pc();
	}
}

static void ANO_Loop_50Hz(void)	//20ms执行一次
{
	ANO_key();				//按键扫描
}

static void ANO_Loop_20Hz(void)	//50ms执行一次
{
	Show.oled_delay = 1;//屏幕显示延时
	NRF_Check_Ch();//自动对频检测
	
	
	if(send_flag)//发送设置数据到飞机	
	ANO_DT_Send_Flag_To_Fly(set_temp,0);
}

static void ANO_Loop_1Hz(void) // 1000ms执行一次
{
	//更新电压值
	Show.Battery_Rc = Rc.AUX5;
	if(Show.Battery_Rc<345 && Show.Battery_Rc>320)  //判断电压提示电池电量低
		Show.low_power = 1;
	else                    
		Show.low_power = 0;
	
	//计算收到飞机数据的帧率
	NRF_SSI = NRF_SSI_CNT;
	Rc.AUX6 = NRF_SSI;
	NRF_SSI_CNT = 0;
	
	/*如果帧率为0标记为失联状态*/
	if(NRF_SSI==0)
	{
		ANO_LED_blue_OFF;
		Show.Rc_num = 0;
		Show.Connect_Succeed = 0;
		Show.hardware_type = 0;
		Show.test_flag = 0;
		Show.windows = 0;
	}
	else
	{
		ANO_LED_blue_ON;
		Show.Connect_Succeed = 1;
	}
}

void ANO_Loop(void)
{
	if(S_cnt_2ms >= 1){
		ANO_Loop_500Hz();
		S_cnt_2ms = 0;
	}		
	if(S_cnt_4ms >= 2){	
		ANO_Loop_250Hz();
		S_cnt_4ms = 0;
	}
	if(S_cnt_10ms >= 5){		
		ANO_Loop_100Hz();
		S_cnt_10ms = 0;
	}
	if(S_cnt_20ms >= 10){		
		ANO_Loop_50Hz();
		S_cnt_20ms = 0;
	}	
	if(S_cnt_50ms >= 25){		
		ANO_Loop_20Hz();
		S_cnt_50ms = 0;
	}	
	if(S_cnt_1000ms >= 500){		
		ANO_Loop_1Hz();
		S_cnt_1000ms = 0;
	}
}
/******************* (C) COPYRIGHT 2014 ANO TECH *****END OF FILE************/
