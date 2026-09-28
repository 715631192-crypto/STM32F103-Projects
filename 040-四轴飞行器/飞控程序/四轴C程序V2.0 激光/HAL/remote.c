
#include "Remote.h"
#include "ALL_DATA.h"
#include "ANO_DT.h"
#include "LED.h"
#include "WIFI_UFO.h"
#include "control.h"
#include "flow.h"
#include "myMath.h"
#include "nrf24l01.h"
#include "spl06.h"
#include <math.h>

#define SUCCESS 0
#undef FAILED
#define FAILED 1

u16 test_flag, set_flag;

void Rc_Connect(void)
{
    // 控制数据优先级
    // 1、遥控器  2、WiFi图传模块
    if (NRF_Connect() == 0)
    {
        if (WIFI_UFO_Connect() == 0)
        {
        }
    }
}

/*****************************************************************************************
 *  通道数据处理
 * @param[in]
 * @param[out]
 * @return
 ******************************************************************************************/
uint8_t RC_rxData[32];
void remote_unlock(void);

void RC_Analy(void)
{
    static uint16_t cnt, cnt_tmp;// 通道数据处理计数器  临时计数器
    /*             Receive  and check RC data                               */
    //	static u8 Connect_flag;
    /// 模式选择飞行程序

    // 控制数据优先级
    if (Nrf_Erro == 1 || WIFI_UFO_Err == 1) // 有数据丢失  则判断是否需要解锁
    {

        {
            //							const float roll_pitch_ratio = 0.04f;
            const float yaw_ratio = 0.0015f; // 通道4  角度控制比例

            //							pidPitch.desired =-(Remote.pitch-1500)*roll_pitch_ratio;	 //将遥杆值作为飞行角度的期望值
            //							pidRoll.desired = -(Remote.roll-1500)*roll_pitch_ratio;

            //							if((Flow_Err ==0) && (mini.ok == 1) && (FlightData.High.bara_height < 40))//光流模块在位  数据有效   //直接控制速度 并且关掉外环计算
            //							{
            //								Flow_SpeedPid_y.desired = -(Remote.pitch-1500)*roll_pitch_ratio;   //摇杆控制速度期望值
            //								Flow_SpeedPid_x.desired = -(Remote.roll-1500) *roll_pitch_ratio;
            //								pidPitch.desired = -(Remote.pitch-1500)*0.02f;  //遥控器摇杆控制姿态外环期望值
            //								pidRoll.desired  = -(Remote.roll-1500) *0.02f;  //遥控器摇杆控制姿态外环期望值
            //							}
            //							else
            //							{
            //								pidPitch.desired = -(Remote.pitch-1500)*roll_pitch_ratio;
            //								pidRoll.desired  = -(Remote.roll-1500) *roll_pitch_ratio;
            //							}
            //
            if (Remote.yaw > 1820)//
            {
                pidYaw.desired -= 0.75f;
            }
            else if (Remote.yaw < 1180)
            {
                pidYaw.desired += 0.75f;
            }
        }
        remote_unlock(); // 解锁判断
    }
    // 如果3秒没收到遥控数据，则判断遥控信号丢失，飞控在任何时候停止飞行，避免伤人。
    // 意外情况，使用者可紧急关闭遥控电源，飞行器会在3秒后立即关闭，避免伤人。
    // 立即关闭遥控，如果在飞行中会直接掉落，可能会损坏飞行器。
    else
    {
        cnt++;
        if (cnt > 800) // 3秒没有遥控器信号 判断遥控器失联 信号断线 自动下降保护
        {

            Remote.roll = 1500; // 通道1    数据归中
            LIMIT(Remote.roll, 1000, 2000);
            Remote.pitch = 1500; // 通道2		数据归中
            LIMIT(Remote.pitch, 1000, 2000);
            Remote.yaw = 1500; // 通道4		数据归中
            LIMIT(Remote.yaw, 1000, 2000);
            if (Remote.thr < 1030) // 判断油门
            {
                cnt = 0;
                Remote.thr = 1000;          // 关闭油门
                ALL_flag.unlock = 0;        // 退出控制
                LED.status = AllFlashLight; // 开始闪灯
                NRF24L01_init();            // 复位一下2.4G模块
            }
            else
            {
                cnt = 810;
                if (cnt_tmp++ > 100) // 控制油门减小的时间
                {
                    cnt_tmp = 0;
                    //							printf("Remote.thr: %d  \r\n",Remote.thr);				//串口1的打印
                    Remote.thr = Remote.thr - 20; // 通道3 油门通道在原来的基础上自动慢慢减小  起到飞机慢慢下降
                }
            }
            LIMIT(Remote.thr, 1000, 2000);
        }
    }
}

/*****************************************************************************************
 *  解锁判断
 * @param[in]
 * @param[out]
 * @return
 ******************************************************************************************/
void remote_unlock(void) // 解锁数据解析
{
    volatile static uint8_t status = WAITING_1;
    static uint16_t cnt = 0;

    if (Remote.thr < 1200 && Remote.yaw > 1800) // 偏航                        //油门遥杆右下角锁定飞机
    {
        status = EXIT_255;
    }

    switch (status)
    {
    case WAITING_1:            // 等待解锁
        if (Remote.thr < 1150) // 解锁三步奏，油门最低->油门最高->油门最低 看到LED灯不闪了 即完成解锁
        {
            status = WAITING_2;
        }
        break;
    case WAITING_2:
        if (Remote.thr > 1800) // 拉高油门
        {
            static uint8_t cnt = 0;
            cnt++;
            if (cnt > 5) // 最高油门需保持200ms以上，防止遥控开机初始化未完成的错误数据
            {
                cnt = 0;
                status = WAITING_3;
            }
        }
        break;
    case WAITING_3:
        if (Remote.thr < 1150) // 拉低油门解锁
        {
            status = WAITING_4; // 解锁标志位
            baro_start = 1;     // 气压清零标志
        }
        break;
    case WAITING_4: // 解锁成功
        ALL_flag.unlock = 1;
        status = PROCESS_31;
        LED.status = AlwaysOn;
        break;
    case PROCESS_31: // 进入解锁状态
        if (Remote.thr < 1020)
        {
            if (cnt++ > 2000) // 解锁后  不动油门遥杆处于最低6S自动上锁
            {
                status = EXIT_255;
            }
        }
        else if (!ALL_flag.unlock) // Other conditions lock
        {
            status = EXIT_255;
        }
        else
            cnt = 0;
        break;
    case EXIT_255:                  // 进入锁定
        LED.status = AllFlashLight; // exit
        cnt = 0;
        LED.FlashTime = 300; // 300*3ms
        ALL_flag.unlock = 0;
        status = WAITING_1;
        break;
    default:
        status = EXIT_255;
        break;
    }
}
/***********************END OF FILE*************************************/
