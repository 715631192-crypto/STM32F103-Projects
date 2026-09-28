#include "show.h"
#include "oledfont.h" 
#include "oled.h"

#define Line1_Begin 29
#define Line2_Begin 5
#define Line3_Begin 5
#define Line4_Begin 30
#define Line5_Begin 2

#define X_Begin 0
#define Y_Begin 51
#define Z_Begin 103

#define Line1_Begin1 0
#define Line2_Begin1 0
#define Line3_Begin1 40
#define Line4_Begin1 0
#define Line5_Begin1 0

#define Y0 0
#define Y1 14
#define Y2 Y1+12
#define Y3 Y2+12
#define Y4 Y3+12
#define Y5 Y4+12

struct _Show Show;

unsigned char i;          //计数变量
unsigned char Send_Count; //串口需要发送的数据个数
float Vol;

/**************************************************************************
函数功能：OLED显示
入口参数：无
返回  值：无
**************************************************************************/
void oled_show(void)
{
	u8 temp;
	int temp1;
	
	static u8 page,page_temp,flash_cnt,show_mode=1;
	
	if(page != page_temp)//切换页面先清屏
	{
		page_temp = page;
		OLED_Clear();
	}
	///////////////////////////////第一行///////////////////////////////////
	//显示提示
	if(Show.low_power)//遥控低电量
	{
		flash_cnt ++;
		if(flash_cnt>3) 
		{
			flash_cnt=0;
			if(show_mode==0)show_mode=1;
			else show_mode = 0;
		}
		
		for(u8 i=0;i<12;i++) OLED_Show_CH_String(Line1_Begin+i*6,Y0,oled_CH16[i],12,show_mode);
	}
	else if(Show.test_flag&BIT5)//飞机低电量
	{
		flash_cnt ++;
		if(flash_cnt>3) 
		{
			flash_cnt=0;
			if(show_mode==0)show_mode=1;
			else show_mode = 0;
		}
		
		for(u8 i=0;i<12;i++) OLED_Show_CH_String(Line1_Begin+i*6,Y0,oled_CH17[i],12,show_mode);
	}
	else
	{
		OLED_Show_CH(Line1_Begin+00,Y0,0,12,1);
		OLED_Show_CH(Line1_Begin+12,Y0,1,12,1);
		OLED_Show_CH(Line1_Begin+24,Y0,2,12,1);
		OLED_Show_CH(Line1_Begin+36,Y0,3,12,1);
		OLED_Show_CH(Line1_Begin+48,Y0,4,12,1);
		OLED_Show_CH(Line1_Begin+60,Y0,5,12,1);
	}
	//////////////////////////////////////////////////////
	if(Show.NRF_Err)	OLED_Show_CH(2,Y0,6 ,12,1);//无线模块故障显示X
	else				OLED_ShowNumber(2,Y0,ANO_Param.NRF_Channel,3,12);//显示无线信道
	
	//显示信号强度
	temp = Show.Rc_num/20;
	switch(temp)
	{
		case 0:OLED_Show_CH(Line1_Begin+85,Y0,6 ,12,1);break;
		case 1:OLED_Show_CH(Line1_Begin+85,Y0,7 ,12,1);break;
		case 2:OLED_Show_CH(Line1_Begin+85,Y0,8 ,12,1);break;
		case 3:OLED_Show_CH(Line1_Begin+85,Y0,9 ,12,1);break;
		case 4:OLED_Show_CH(Line1_Begin+85,Y0,10,12,1);break;
		default:OLED_Show_CH(Line1_Begin+85,Y0,11,12,1);break;
	}
	///////////////////////////////第二行///////////////////////////////////
	//显示电压	
	OLED_ShowString(Line2_Begin+00,Y1,"R-V:",12,1);
	OLED_ShowString(Line2_Begin+36,Y1,".",12,1);
	OLED_ShowNumber(Line2_Begin+30,Y1,Show.Battery_Rc/100,1,12);
	OLED_ShowNumber(Line2_Begin+42,Y1,Show.Battery_Rc%100,2,12);
	if(Show.Battery_Rc%100<10)	OLED_ShowNumber(Line2_Begin+42,Y1,0,1,12);
	
	OLED_ShowString(Line2_Begin+64,Y1,"F-V:",12,1);
	OLED_ShowString(Line2_Begin+100,Y1,".",12,1);
	OLED_ShowNumber(Line2_Begin+94,Y1,Show.Battery_Fly/100,1,12);
	OLED_ShowNumber(Line2_Begin+106,Y1,Show.Battery_Fly%100,2,12);
	if(Show.Battery_Fly%100<10)	OLED_ShowNumber(Line2_Begin+106,Y1,0,1,12);
	///////////////////////////////第三、四行/////////////////////////////////
	//显示遥控数据
	OLED_ShowString(Line3_Begin+00,Y2,"THR:",12,1);
	temp = (Rc.THR-1000)/41;
	OLED_Show_progress_bar(temp,12,24,Line4_Begin+6 ,Y2,12,1);
	temp = (Rc.THR-1500)/41;
	OLED_Show_progress_bar(temp,12,12,Line4_Begin+18,Y2,12,1);
	
	OLED_ShowString(Line3_Begin+64,Y2,"ROL:",12,1);
	temp = (Rc.ROL-1000)/41;
	OLED_Show_progress_bar(temp,12,24,Line4_Begin+70,Y2,12,1);
	temp = (Rc.ROL-1500)/41;
	OLED_Show_progress_bar(temp,12,12,Line4_Begin+82,Y2,12,1);
	
	OLED_ShowString(Line3_Begin+00,Y3,"YAW:",12,1);
	temp = (Rc.YAW-1000)/41;
	OLED_Show_progress_bar(temp,12,24,Line4_Begin+6 ,Y3,12,1);
	temp = (Rc.YAW-1500)/41;
	OLED_Show_progress_bar(temp,12,12,Line4_Begin+18,Y3,12,1);
	
	OLED_ShowString(Line3_Begin+64,Y3,"PIT:",12,1);
	temp = (Rc.PIT-1000)/41;
	OLED_Show_progress_bar(temp,12,24,Line4_Begin+70,Y3,12,1);
	temp = (Rc.PIT-1500)/41;
	OLED_Show_progress_bar(temp,12,12,Line4_Begin+82,Y3,12,1);
	///////////////////////////////第五行///////////////////////////////////
	if(Show.Rc_num && Show.hardware_type == 1)
	{
		page = 1;//第一页
		//显示姿态角度和高度
		if(Show.X<0) temp1 = -Show.X,OLED_ShowString(X_Begin,Y4,"-",12,1);
		else         temp1 = Show.X, OLED_ShowString(X_Begin,Y4,"+",12,1);
		OLED_ShowNumber(X_Begin+8,Y4,temp1/10,3,12);
		OLED_ShowString(X_Begin+28,Y4,".",12,1);
		OLED_ShowNumber(X_Begin+32,Y4,temp1%10,1,12);
		
		if(Show.Y<0) temp1 = -Show.Y,OLED_ShowString(Y_Begin,Y4,"-",12,1);
		else    temp1 = Show.Y, OLED_ShowString(Y_Begin,Y4,"+",12,1);
		OLED_ShowNumber(Y_Begin+8,Y4,temp1/10,3,12);
		OLED_ShowString(Y_Begin+28,Y4,".",12,1);
		OLED_ShowNumber(Y_Begin+32,Y4,temp1%10,1,12);
		
		if(Show.H<0) 			temp1 =-Show.H,  OLED_ShowString(Z_Begin,Y4,"-",12,1);
		else    			temp1 = Show.H,  OLED_ShowString(Z_Begin,Y4,"+",12,1);
		//最高显示999厘米,也就是9.99米。
		if(temp1>999) temp1 = 999,OLED_ShowString(Z_Begin,Y4,">",12,1);
		OLED_ShowNumber(Z_Begin+8,Y4,temp1,3,12);
	}
	else
	{
		page = 2;//第二页
		//显示微调旋钮数据
		OLED_ShowString(Line5_Begin+00,Y4,"R:",12,1);
		temp = (ANO_Param.OffSet_Rol-1000)/41;
		OLED_Show_progress_bar(temp,12,24,Line5_Begin+15 ,Y4,12,1);
		temp = (ANO_Param.OffSet_Rol-1500)/41;
		OLED_Show_progress_bar(temp,12,12,Line5_Begin+27,Y4,12,1);
		
		OLED_ShowString(Line5_Begin+44,Y4,"Y:",12,1);
		temp = (ANO_Param.OffSet_Yaw-1000)/41;
		OLED_Show_progress_bar(temp,12,24,Line5_Begin+59 ,Y4,12,1);
		temp = (ANO_Param.OffSet_Yaw-1500)/41;
		OLED_Show_progress_bar(temp,12,12,Line5_Begin+71,Y4,12,1);
		
		OLED_ShowString(Line5_Begin+88,Y4,"P:",12,1);
		temp = (ANO_Param.OffSet_Pit-1000)/41;
		OLED_Show_progress_bar(temp,12,24,Line5_Begin+103 ,Y4,12,1);
		temp = (ANO_Param.OffSet_Pit-1500)/41;
		OLED_Show_progress_bar(temp,12,12,Line5_Begin+115,Y4,12,1);
	}
	
	OLED_Refresh_Gram();//开始显示
}

//进度条显示函数
void OLED_Show_progress_bar(u8 temp,u8 chr_star,u8 chr_default,u8 x,u8 y,u8 size,u8 mode)
{
	switch(temp)
	{
		case  0:OLED_Show_CH(x,y,chr_star+temp,size,size);break;
		case  1:OLED_Show_CH(x,y,chr_star+temp,size,size);break;
		case  2:OLED_Show_CH(x,y,chr_star+temp,size,size);break;
		case  3:OLED_Show_CH(x,y,chr_star+temp,size,size);break;
		case  4:OLED_Show_CH(x,y,chr_star+temp,size,size);break;
		case  5:OLED_Show_CH(x,y,chr_star+temp,size,size);break;
		case  6:OLED_Show_CH(x,y,chr_star+temp,size,size);break;
		case  7:OLED_Show_CH(x,y,chr_star+temp,size,size);break;
		case  8:OLED_Show_CH(x,y,chr_star+temp,size,size);break;
		case  9:OLED_Show_CH(x,y,chr_star+temp,size,size);break;
		case 10:OLED_Show_CH(x,y,chr_star+temp,size,size);break;
		case 11:OLED_Show_CH(x,y,chr_star+temp,size,size);break;
		case 12:OLED_Show_CH(x,y,chr_star+temp,size,size);break;
		
		default:OLED_Show_CH(x,y,chr_default,size,size);break;
	}
}

vs16 set_bit=0,bit_max=5;
u16 set_temp=0x0000;

void OLED_Show_Seting(void)
{
	u8 cnt=0,bit_cnt=0;
	u8 mode,page;
	static u8 page_temp;
	
	for(u8 i=0;i<8;i++) OLED_Show_CH_String(40+i*6,cnt,oled_CH0[i],12,1);

	page = set_bit/4;
	bit_cnt = 4*page;
	if(page_temp!=page)
	{
		page_temp=page;
		OLED_Clear();
	}
	
	switch(page)
	{
		case 0:
			cnt+=13;
			for(u8 i=0;i<10;i++) OLED_Show_CH_String(0+i*6,cnt,oled_CH1[i],12,1);
			if(set_bit==bit_cnt) mode=0;	else mode=1;
			if( set_temp&BIT0 )	for(u8 i=0;i<6;i++) OLED_Show_CH_String(94+i*6,cnt,oled_CH12[i],12,mode);
			else								for(u8 i=0;i<6;i++) OLED_Show_CH_String(94+i*6,cnt,oled_CH13[i],12,mode);
			bit_cnt++;

			cnt+=13;
			for(u8 i=0;i<10;i++) OLED_Show_CH_String(0+i*6,cnt,oled_CH2[i],12,1);
			if(set_bit==bit_cnt) mode=0;	else mode=1;
			if( set_temp&BIT1 )	for(u8 i=0;i<6;i++) OLED_Show_CH_String(94+i*6,cnt,oled_CH12[i],12,mode);
			else								for(u8 i=0;i<6;i++) OLED_Show_CH_String(94+i*6,cnt,oled_CH13[i],12,mode);
			bit_cnt++;

			cnt+=13;
			for(u8 i=0;i<10;i++) OLED_Show_CH_String(0+i*6,cnt,oled_CH3[i],12,1);
			if(set_bit==bit_cnt) mode=0; else mode=1;
			if( set_temp&BIT2 )	for(u8 i=0;i<6;i++) OLED_Show_CH_String(94+i*6,cnt,oled_CH12[i],12,mode);
			else								for(u8 i=0;i<6;i++) OLED_Show_CH_String(94+i*6,cnt,oled_CH13[i],12,mode);
			bit_cnt++;

			cnt+=13;
			for(u8 i=0;i<10;i++) OLED_Show_CH_String(0+i*6,cnt,oled_CH4[i],12,1);
			if(set_bit==bit_cnt)	mode=0; else mode=1;
			if( set_temp&BIT3 )	for(u8 i=0;i<6;i++) OLED_Show_CH_String(94+i*6,cnt,oled_CH12[i],12,mode);
			else								for(u8 i=0;i<6;i++) OLED_Show_CH_String(94+i*6,cnt,oled_CH13[i],12,mode);
			bit_cnt++;
		break;
		
		case 1:
			cnt+=13;
			for(u8 i=0;i<10;i++) OLED_Show_CH_String(0+i*6,cnt,oled_CH14[i],12,1);
			if(set_bit==bit_cnt)	mode=0; else mode=1;
			if( set_temp&BIT4 )	for(u8 i=0;i<6;i++) OLED_Show_CH_String(94+i*6,cnt,oled_CH12[i],12,mode);
			else								for(u8 i=0;i<6;i++) OLED_Show_CH_String(94+i*6,cnt,oled_CH13[i],12,mode);
			bit_cnt++;
		
			cnt+=13;
			for(u8 i=0;i<10;i++) OLED_Show_CH_String(0+i*6,cnt,oled_CH18[i],12,1);
			if(set_bit==bit_cnt)	mode=0; else mode=1;
			if( set_temp&BIT5 )	for(u8 i=0;i<6;i++) OLED_Show_CH_String(94+i*6,cnt,oled_CH12[i],12,mode);
			else								for(u8 i=0;i<6;i++) OLED_Show_CH_String(94+i*6,cnt,oled_CH13[i],12,mode);
			bit_cnt++;
		
		break;
		
		default:break;
	}
	
	OLED_Refresh_Gram();//开始显示
}

vs16 test_bit=0,test_max=5;
u16  test_temp=0;

void OLED_Show_Test(void)
{
	u8 cnt=0,bit_cnt=0;
	u8 mode,page;
	static u8 page_temp;
	
	test_temp = Show.test_flag;
	
	for(u8 i=0;i<10;i++) OLED_Show_CH_String(34+i*6,cnt,oled_CH5[i],12,1);
	
	page = test_bit/4;
	bit_cnt = 4*page;
	if(page_temp!=page)	
	{
		page_temp=page;
		OLED_Clear();
	}
	
	switch(page)
	{
		case 0:
			cnt+=13;
			for(u8 i=0;i<8;i++) OLED_Show_CH_String(0+i*6,cnt,oled_CH6[i],12,1);
			if(test_bit==bit_cnt++) mode=0; else mode=1;
			if( test_temp&BIT0 )for(u8 i=0;i<4;i++) OLED_Show_CH_String(104+i*6,cnt,oled_CH10[i],12,mode);
			else								for(u8 i=0;i<4;i++) OLED_Show_CH_String(104+i*6,cnt,oled_CH11[i],12,mode);
			
			cnt+=13;
			for(u8 i=0;i<8;i++) OLED_Show_CH_String(0+i*6,cnt,oled_CH7[i],12,1);
			if(test_bit==bit_cnt++) mode=0; else mode=1;
			if( test_temp&BIT1 )for(u8 i=0;i<4;i++) OLED_Show_CH_String(104+i*6,cnt,oled_CH10[i],12,mode);
			else								for(u8 i=0;i<4;i++) OLED_Show_CH_String(104+i*6,cnt,oled_CH11[i],12,mode);
			
			cnt+=13;
			for(u8 i=0;i<10;i++) OLED_Show_CH_String(0+i*6,cnt,oled_CH8[i],12,1);
			if(test_bit==bit_cnt++) mode=0; else mode=1;
			if( test_temp&BIT2 )for(u8 i=0;i<4;i++) OLED_Show_CH_String(104+i*6,cnt,oled_CH10[i],12,mode);
			else								for(u8 i=0;i<4;i++) OLED_Show_CH_String(104+i*6,cnt,oled_CH11[i],12,mode);
			
			cnt+=13;
			for(u8 i=0;i<12;i++) OLED_Show_CH_String(0+i*6,cnt,oled_CH9[i],12,1);
			if(test_bit==bit_cnt++) mode=0; else mode=1;
			if( test_temp&BIT3 )for(u8 i=0;i<4;i++) OLED_Show_CH_String(104+i*6,cnt,oled_CH10[i],12,mode);
			else								for(u8 i=0;i<4;i++) OLED_Show_CH_String(104+i*6,cnt,oled_CH11[i],12,mode);
		break;
		
		case 1:
			cnt+=13;
			for(u8 i=0;i<14;i++) OLED_Show_CH_String(0+i*6,cnt,oled_CH15[i],12,1);
			if(test_bit==bit_cnt++) mode=0; else mode=1;
			if( test_temp&BIT4 )for(u8 i=0;i<4;i++) OLED_Show_CH_String(104+i*6,cnt,oled_CH10[i],12,mode);
			else								for(u8 i=0;i<4;i++) OLED_Show_CH_String(104+i*6,cnt,oled_CH11[i],12,mode);
		
			cnt+=13;
			for(u8 i=0;i<10;i++) OLED_Show_CH_String(0+i*6,cnt,oled_CH19[i],12,1);
			if(test_bit==bit_cnt++) mode=0; else mode=1;
			if( test_temp&BIT6 )for(u8 i=0;i<4;i++) OLED_Show_CH_String(104+i*6,cnt,oled_CH10[i],12,mode);
			else								for(u8 i=0;i<4;i++) OLED_Show_CH_String(104+i*6,cnt,oled_CH11[i],12,mode);
		break;
		
		default:break;
	}

	OLED_Refresh_Gram();//开始显示
}

//遥杆检测
void Gesture_Check0(u16 val,vs16 *set,vs16 max)  //摇杆摇杆上下移动选择定高定点控制
{
	static u8 cnt0,cnt1;
	
	if(val>1800) 
	{
		cnt0++;
	}
	else
	{
		if(cnt0>2) (*set)--;
		cnt0 = 0;
	}
	
	if(val<1200) 
	{
		cnt1++;
	}
	else
	{
		if(cnt1>2) (*set)++;
		cnt1 = 0;
	}
	
	if((*set)<0) 				(*set) = max;
	else if((*set)>max) (*set) = 0;
}

void Gesture_Check1(u16 val,u16 *set,vs16 bit)
{
	static u8 cnt0;
	
	if(val>1800 || val<1200) 
	{
		cnt0++;
	}
	else
	{
		if(cnt0>2) 
			*set = REVERSE(*set,bit);
		cnt0 = 0;
	}
}

u8 send_flag=0;

void Gesture_Check(void)//遥杆检测
{
	static u8 temp;
	
	if(temp!=Show.windows)//如果窗口改变
	{
		if(Show.windows==1) set_temp = Show.set_flag;
		if(temp==1 && set_temp!=Show.set_flag) send_flag = 1;
		temp=Show.windows;
	}
	switch(Show.windows)//根据窗口显示不同的遥杆
	{
		case 1:
			Gesture_Check0(Rc.PIT,&set_bit,bit_max);//摇杆上下移动选择定高定点控制杆
			if(Show.Connect_Succeed)
			{
				Gesture_Check1(Rc.ROL,&set_temp,set_bit);
			}
		break;
		
		case 2:
			Gesture_Check0(Rc.PIT,&test_bit,test_max);//测试杆上下移动选择定高定点控制杆
		break;
		
		default:break;
	}
}

void Show_Duty(void)
{
	static u8 temp;
	
	if(Show.windows!=temp)
	{
		temp = Show.windows;
		OLED_Clear();
	}
	switch(Show.windows)
	{
		case 0:	oled_show();	break;
		case 1:	OLED_Show_Seting();	break;
		case 2:	OLED_Show_Test();	break;
		default:break;
	}
}
