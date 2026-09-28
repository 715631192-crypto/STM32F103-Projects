/******************** (C) COPYRIGHT 2016 ANO Tech ***************************

*****************************************************************************/
#include "ANO_Data_Transfer.h"
#include "ADC.h"
#include "ALL_DATA.h"
#include "ANO_DT.h"
#include "LED.h"
#include "Remote.h"
#include "SPI.h"
#include "flow.h"
#include "nrf24l01.h"
#include "spl06.h"
#include <string.h>

dt_flag_t f;         // 需要发送数据的标志
u8 data_to_send[50]; // 发送数据缓存

// Send_Data函数是协议中所有发送数据功能使用到的发送函数
// 移植时，用户应根据自身应用的情况，根据使用的通信方式，实现此函数

void ANO_DT_Send_Data(u8 *dataToSend, u8 length)
{
    //	switch(flag.NS)
    //	{
    //		case 1://已连接遥控器
    ANO_NRF_TxPacket_AP(dataToSend, length);
    //		break;
    //
    //		case 3://已连接蓝牙模块
    ////			ANO_UART3_Put_Buf(dataToSend,length);
    //		break;
    //
    ////		default:break;
    //	}
    //	Usb_Hid_Adddata(dataToSend,length);
    //	Usb_Hid_Send();
}
///////////////////////////////////////////////////////////////////////////////////////
////Data_Exchange函数处理各种数据发送请求，比如想实现每6ms发送一次传感器数据至上位机，即在此函数内实现(cnt = 6/2 = 3)
////此函数应由用户每2ms调用一次
u8 cnt = 0, yaw_lock = 0, Send_Check = 0;

void ANO_DT_Data_Exchange(void) // 飞机发送数据到遥控器上传到上位机
{
    if (Send_Check) // 有数据需要发送
    {
        Send_Check = 0;
        ANO_DT_Send_Data(data_to_send, 7);
    }
    else if (f.send_pid) /// 上位机获取PID参数
    {
        cnt++; // 每2ms发送一次PID参数，共4次
        switch (cnt)
        {
        case 1:
            ANO_DT_Send_PID(1, pidRateX.kp * 1000, pidRateX.ki * 1000, pidRateX.kd * 1000,
                            pidRateY.kp * 1000, pidRateY.ki * 1000, pidRateY.kd * 1000,
                            pidRateZ.kp * 1000, pidRateZ.ki * 1000, pidRateZ.kd * 1000);
            break;
        case 2:
            ANO_DT_Send_PID(2, pidRoll.kp * 1000, pidRoll.ki * 1000, pidRoll.kd * 1000,
                            pidPitch.kp * 1000, pidPitch.ki * 1000, pidPitch.kd * 1000,
                            pidYaw.kp * 1000, pidYaw.ki * 1000, pidYaw.kd * 1000);
            //			cnt = 0;	f.send_pid = 0;
            break;
        case 3:
            ANO_DT_Send_PID(3, pidHeightRate.kp * 1000, pidHeightRate.ki * 1000, pidHeightRate.kd * 1000,
                            pidHeightHigh.kp * 1000, pidHeightHigh.ki * 1000, pidHeightHigh.kd * 1000,
                            Flow_SpeedPid_x.kp * 1000, Flow_SpeedPid_x.ki * 1000, Flow_SpeedPid_x.kd * 1000);
            // cnt = 0;	f.send_pid = 0;
            break;
        case 4:
            ANO_DT_Send_PID(4, Flow_PosPid_x.kp * 1000, Flow_PosPid_x.ki * 1000, Flow_PosPid_x.kd * 1000,
                            Flow_SpeedPid_y.kp * 1000, Flow_SpeedPid_y.ki * 1000, Flow_SpeedPid_y.kd * 1000,
                            Flow_PosPid_y.kp * 1000, Flow_PosPid_y.ki * 1000, Flow_PosPid_y.kd * 1000);
            cnt = 0;
            f.send_pid = 0;
            break;
        }
    }
    else if (f.send_version) // 上位机获取飞机版本
    {
        f.send_version = 0;
        // ANO_DT_Send_Version(1,ANO_Param.hardware,ANO_Param.software,510,0);
    }
    else // 无数据需要发送
    {
        cnt++;
        switch (cnt)
        {
        case 1:
            ANO_DT_Send_Status(-Angle.roll, Angle.pitch, -Angle.yaw, mini.flow_High, ALL_flag.slock_flag, ALL_flag.unlock); // 飞控状态    10001
            break;
        case 2:
            ANO_DT_Send_Senser(MPU6050.accX, MPU6050.accY, MPU6050.accZ,       //	陀螺仪加速度数据
                               MPU6050.gyroX, MPU6050.gyroY, MPU6050.gyroZ,    //	陀螺仪角度数据
                               mini.flow_x_iOUT, mini.flow_y_iOUT, mini.qual); //	光流原使数据
            break;
        case 3:
            ANO_DT_Send_RCData(Remote.thr, Remote.yaw, Remote.roll, Remote.pitch, Remote.AUX1, Remote.AUX2, Remote.AUX3, Remote.AUX4, NRF_SSI, Remote.AUX6); // 遥控器通道数据
            break;
        case 4:
            ANO_DT_Send_Power(voltage / 10, Remote.AUX5, 1, NRF_SSI, test_flag, set_flag); // 飞机电压/遥控器电压/无线模块的信号值/传感器状态/飞行模式
            break;
        case 5:
            ANO_DT_Send_Senser2(FlightData.High.bara_height, Control_high); // 气压计/实时控制的高度
            break;
        case 6:
            ANO_DT_Send_speed(pixel_flow.loc_x, pixel_flow.loc_y, FlightData.High.bara_height);
            cnt = 0; // 光流模块数据
            break;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////
////Data_Receive_Anl函数是协议数据解析函数，函数参数是符合协议格式的一个数据帧，该函数会首先对协议数据进行校验
////校验通过后对数据进行解析，实现相应功能
////此函数可以不用用户自行调用，由函数Data_Receive_Prepare自动调用

extern u16 save_pid_en;

void ANO_DT_Send_Version(u8 hardware_type, u16 hardware_ver, u16 software_ver, u16 protocol_ver, u16 bootloader_ver)
{
    u8 _cnt = 0;
    u8 sum = 0;
    u8 i;
    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0x00;
    data_to_send[_cnt++] = 0;

    data_to_send[_cnt++] = hardware_type;
    data_to_send[_cnt++] = BYTE1(hardware_ver);
    data_to_send[_cnt++] = BYTE0(hardware_ver);
    data_to_send[_cnt++] = BYTE1(software_ver);
    data_to_send[_cnt++] = BYTE0(software_ver);
    data_to_send[_cnt++] = BYTE1(protocol_ver);
    data_to_send[_cnt++] = BYTE0(protocol_ver);
    data_to_send[_cnt++] = BYTE1(bootloader_ver);
    data_to_send[_cnt++] = BYTE0(bootloader_ver);

    data_to_send[3] = _cnt - 4;

    for (i = 0; i < _cnt; i++)
        sum += data_to_send[i];
    data_to_send[_cnt++] = sum;

    ANO_DT_Send_Data(data_to_send, _cnt);
}

void ANO_DT_Send_Speed(float x_s, float y_s, float z_s)
{
    u8 _cnt = 0;
    vs16 _temp;
    u8 sum = 0;
    u8 i;
    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0x0B;
    data_to_send[_cnt++] = 0;

    _temp = (int)(0.1f * x_s);
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);
    _temp = (int)(0.1f * y_s);
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);
    _temp = (int)(0.1f * z_s);
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);

    data_to_send[3] = _cnt - 4;

    for (i = 0; i < _cnt; i++)
        sum += data_to_send[i];
    data_to_send[_cnt++] = sum;

    ANO_DT_Send_Data(data_to_send, _cnt);
}

void ANO_DT_Send_Location(void)
{
    u8 _cnt = 0;
    vs16 _temp;
    u8 sum = 0;
    u8 i;
    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0x32;
    data_to_send[_cnt++] = 0;

    data_to_send[_cnt++] = 0;

    //	_temp = (s16)(-Rol_Data*100);
    //	data_to_send[_cnt++]=BYTE1(_temp);
    //	data_to_send[_cnt++]=BYTE0(_temp);
    //
    //	_temp = (s16)(Pit_Data*100);
    //	data_to_send[_cnt++]=BYTE1(_temp);
    //	data_to_send[_cnt++]=BYTE0(_temp);

    //	_temp = (s16)(Line_Angle*100);
    //	data_to_send[_cnt++]=BYTE1(_temp);
    //	data_to_send[_cnt++]=BYTE0(_temp);

    data_to_send[3] = _cnt - 4;

    for (i = 0; i < _cnt; i++)
        sum += data_to_send[i];

    data_to_send[_cnt++] = sum;

    ANO_DT_Send_Data(data_to_send, _cnt);
}

void ANO_DT_Send_Status(float angle_rol, float angle_pit, float angle_yaw, s32 alt, u8 fly_model, u8 armed)
{
    u8 _cnt = 0;
    vs16 _temp;
    vs32 _temp2 = alt;
    u8 sum = 0;
    u8 i;

    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0x01;
    data_to_send[_cnt++] = 0;

    _temp = (int)(angle_rol * 100);
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);
    _temp = (int)(angle_pit * 100);
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);
    _temp = (int)(angle_yaw * 100);
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);

    data_to_send[_cnt++] = BYTE3(_temp2);
    data_to_send[_cnt++] = BYTE2(_temp2);
    data_to_send[_cnt++] = BYTE1(_temp2);
    data_to_send[_cnt++] = BYTE0(_temp2);

    data_to_send[_cnt++] = fly_model;

    data_to_send[_cnt++] = armed;

    data_to_send[3] = _cnt - 4;

    for (i = 0; i < _cnt; i++)
        sum += data_to_send[i];
    data_to_send[_cnt++] = sum;

    ANO_DT_Send_Data(data_to_send, _cnt);
}
void ANO_DT_Send_Senser(s16 a_x, s16 a_y, s16 a_z, s16 g_x, s16 g_y, s16 g_z, s16 m_x, s16 m_y, s16 m_z)
{
    u8 _cnt = 0;
    vs16 _temp;
    u8 sum = 0;
    u8 i;

    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0x02;
    data_to_send[_cnt++] = 0;

    _temp = a_x;
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);
    _temp = a_y;
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);
    _temp = a_z;
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);

    _temp = g_x;
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);
    _temp = g_y;
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);
    _temp = g_z;
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);

    _temp = m_x;
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);
    _temp = m_y;
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);
    _temp = m_z;
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);
    /////////////////////////////////////////
    _temp = 0;
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);

    data_to_send[3] = _cnt - 4;

    for (i = 0; i < _cnt; i++)
        sum += data_to_send[i];
    data_to_send[_cnt++] = sum;

    ANO_DT_Send_Data(data_to_send, _cnt);
}
////////////////////////////////////////////////////////
void ANO_DT_Send_Senser2(s32 bar_alt, u16 csb_alt)
{
    u8 _cnt = 0;
    u8 sum = 0;
    u8 i;

    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0x07;
    data_to_send[_cnt++] = 0;

    data_to_send[_cnt++] = BYTE3(bar_alt);
    data_to_send[_cnt++] = BYTE2(bar_alt);
    data_to_send[_cnt++] = BYTE1(bar_alt);
    data_to_send[_cnt++] = BYTE0(bar_alt);

    data_to_send[_cnt++] = BYTE1(csb_alt);
    data_to_send[_cnt++] = BYTE0(csb_alt);

    data_to_send[3] = _cnt - 4;

    for (i = 0; i < _cnt; i++)
        sum += data_to_send[i];
    data_to_send[_cnt++] = sum;

    ANO_DT_Send_Data(data_to_send, _cnt);
}

///////////////////////////////////////////////////////////////////////////////////
void ANO_DT_Send_RCData(u16 thr, u16 yaw, u16 rol, u16 pit, u16 aux1, u16 aux2, u16 aux3, u16 aux4, u16 aux5, u16 aux6)
{
    u8 _cnt = 0;
    u8 sum = 0;
    u8 i;
    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0x03;
    data_to_send[_cnt++] = 0;
    data_to_send[_cnt++] = BYTE1(thr);
    data_to_send[_cnt++] = BYTE0(thr);
    data_to_send[_cnt++] = BYTE1(yaw);
    data_to_send[_cnt++] = BYTE0(yaw);
    data_to_send[_cnt++] = BYTE1(rol);
    data_to_send[_cnt++] = BYTE0(rol);
    data_to_send[_cnt++] = BYTE1(pit);
    data_to_send[_cnt++] = BYTE0(pit);
    data_to_send[_cnt++] = BYTE1(aux1);
    data_to_send[_cnt++] = BYTE0(aux1);
    data_to_send[_cnt++] = BYTE1(aux2);
    data_to_send[_cnt++] = BYTE0(aux2);
    data_to_send[_cnt++] = BYTE1(aux3);
    data_to_send[_cnt++] = BYTE0(aux3);
    data_to_send[_cnt++] = BYTE1(aux4);
    data_to_send[_cnt++] = BYTE0(aux4);
    data_to_send[_cnt++] = BYTE1(aux5);
    data_to_send[_cnt++] = BYTE0(aux5);
    data_to_send[_cnt++] = BYTE1(aux6);
    data_to_send[_cnt++] = BYTE0(aux6);

    data_to_send[3] = _cnt - 4;

    for (i = 0; i < _cnt; i++)
        sum += data_to_send[i];

    data_to_send[_cnt++] = sum;

    ANO_DT_Send_Data(data_to_send, _cnt);
}

void ANO_DT_Send_Power(u16 votage, u16 current, u8 flag0, u8 flag1, u16 flag2, u16 flag3)
{
    u8 _cnt = 0;
    u16 temp;
    u8 sum = 0;
    u8 i;

    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0x05;
    data_to_send[_cnt++] = 0;

    temp = votage;
    data_to_send[_cnt++] = BYTE1(temp);
    data_to_send[_cnt++] = BYTE0(temp);

    temp = current;
    data_to_send[_cnt++] = BYTE1(temp);
    data_to_send[_cnt++] = BYTE0(temp);

    data_to_send[_cnt++] = flag0;
    data_to_send[_cnt++] = flag1;

    temp = flag2;
    data_to_send[_cnt++] = BYTE1(temp);
    data_to_send[_cnt++] = BYTE0(temp);

    temp = flag3;
    data_to_send[_cnt++] = BYTE1(temp);
    data_to_send[_cnt++] = BYTE0(temp);

    data_to_send[3] = _cnt - 4;

    for (i = 0; i < _cnt; i++)
        sum += data_to_send[i];

    data_to_send[_cnt++] = sum;

    ANO_DT_Send_Data(data_to_send, _cnt);
}
void ANO_DT_Send_MotoPWM(u16 m_1, u16 m_2, u16 m_3, u16 m_4, u16 m_5, u16 m_6, u16 m_7, u16 m_8)
{
    u8 _cnt = 0;
    u8 sum = 0;
    u8 i;

    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0x06;
    data_to_send[_cnt++] = 0;

    data_to_send[_cnt++] = BYTE1(m_1);
    data_to_send[_cnt++] = BYTE0(m_1);
    data_to_send[_cnt++] = BYTE1(m_2);
    data_to_send[_cnt++] = BYTE0(m_2);
    data_to_send[_cnt++] = BYTE1(m_3);
    data_to_send[_cnt++] = BYTE0(m_3);
    data_to_send[_cnt++] = BYTE1(m_4);
    data_to_send[_cnt++] = BYTE0(m_4);
    data_to_send[_cnt++] = BYTE1(m_5);
    data_to_send[_cnt++] = BYTE0(m_5);
    data_to_send[_cnt++] = BYTE1(m_6);
    data_to_send[_cnt++] = BYTE0(m_6);
    data_to_send[_cnt++] = BYTE1(m_7);
    data_to_send[_cnt++] = BYTE0(m_7);
    data_to_send[_cnt++] = BYTE1(m_8);
    data_to_send[_cnt++] = BYTE0(m_8);

    data_to_send[3] = _cnt - 4;

    for (i = 0; i < _cnt; i++)
        sum += data_to_send[i];

    data_to_send[_cnt++] = sum;

    ANO_DT_Send_Data(data_to_send, _cnt);
}

void ANO_DT_Send_speed(u16 speed_rol_x, u16 speed_pit_y, u16 speed_z)
{
    u8 _cnt = 0;
    u16 temp;
    u8 sum = 0;
    u8 i;

    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0x0B;
    data_to_send[_cnt++] = 0;

    temp = speed_rol_x;
    data_to_send[_cnt++] = BYTE1(temp);
    data_to_send[_cnt++] = BYTE0(temp);

    temp = speed_pit_y;
    data_to_send[_cnt++] = BYTE1(temp);
    data_to_send[_cnt++] = BYTE0(temp);

    temp = speed_z;
    data_to_send[_cnt++] = BYTE1(temp);
    data_to_send[_cnt++] = BYTE0(temp);

    data_to_send[3] = _cnt - 4;

    for (i = 0; i < _cnt; i++)
        sum += data_to_send[i];

    data_to_send[_cnt++] = sum;

    ANO_DT_Send_Data(data_to_send, _cnt);
}

void ANO_DT_Send_PID(u8 group, s16 p1_p, s16 p1_i, s16 p1_d, s16 p2_p, s16 p2_i, s16 p2_d, s16 p3_p, s16 p3_i, s16 p3_d)
{// 发送PID参数
    u8 _cnt = 0;
    vs16 _temp;
    u8 sum = 0;
    u8 i;

    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0xAA;
    data_to_send[_cnt++] = 0x10 + group - 1;
    data_to_send[_cnt++] = 0;

    _temp = p1_p;
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);
    _temp = p1_i;
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);
    _temp = p1_d;
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);
    _temp = p2_p;
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);
    _temp = p2_i;
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);
    _temp = p2_d;
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);
    _temp = p3_p;
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);
    _temp = p3_i;
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);
    _temp = p3_d;
    data_to_send[_cnt++] = BYTE1(_temp);
    data_to_send[_cnt++] = BYTE0(_temp);

    data_to_send[3] = _cnt - 4;

    for (i = 0; i < _cnt; i++)
        sum += data_to_send[i];

    data_to_send[_cnt++] = sum;

    ANO_DT_Send_Data(data_to_send, _cnt);
}

void Flag_Check(void)
{
    test_flag = 0;

    if (!MPU_Err)
        test_flag |= BIT0;
    if (!SPL_Err)
        test_flag |= BIT1;
    if (!NRF_Err)
        test_flag |= BIT2;
    if (ult_ok)
        test_flag |= BIT3;
    if (!Locat_Err)
        test_flag |= BIT4;
    if (LED_warn == 1)
        test_flag |= BIT5; // 低压
    if (!Flow_Err)
        test_flag |= BIT6;
}

///******************* (C) COPYRIGHT 2016 ANO TECH *****END OF FILE************/
