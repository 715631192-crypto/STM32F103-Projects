
#include "control.h"
#include "ALL_DATA.h"
#include "ALL_DEFINE.h"
#include "attitude_process.h"
#include "flow.h"
#include "pid.h"
#include "spl06.h"
//------------------------------------------------------------------------------
#undef NULL
#define NULL 0
#undef DISABLE
#define DISABLE 0
#undef ENABLE
#define ENABLE 1
#undef REST
#define REST 0
#undef SET
#define SET 1
#undef EMERGENT
#define EMERGENT 0

#define REMOTE_THR Remote.thr
#define REMOTE_PITCH Remote.pitch
#define REMOTE_ROLL Remote.roll
#define REMOTE_YAW Remote.yaw
// #define measured FeedBack
#define Expect desired

float Throttle_out; // 飞控油门输出值 //遥控 + 定高 输出值
//------------------------------------------------------------------------------
PidObject *(pPidObject[]) = {&pidRateX, &pidRateY, &pidRateZ, &pidRoll, &pidPitch, &pidYaw // 结构体数组，将每一个数组放一个pid结构体，这样就可以批量操作各个PID的数据了  比如解锁时批量复位pid控制数据，新手明白这句话的作用就可以了
                             ,
                             &pidHeightRate, &pidHeightHigh, &Flow_SpeedPid_x, &Flow_PosPid_x, &Flow_SpeedPid_y, &Flow_PosPid_y};
/**************************************************************
 *  Height control
 * @param[in]
 * @param[out]
 * @return
 ***************************************************************/

// static uint8_t set_high_desired = 0; //定高高度已设定

int16_t HIGH_START = 180; // 一键起飞目标高度

uint32_t Control_high = 0; // 当前高度
/**************************************************************
 *  //高度控制器     气压
 * @param[in]
 * @param[out]
 * @return
 ***************************************************************/
void HeightPidControl(float dt) // 高度控制器     气压
{
    volatile static uint8_t status = WAITING_1;
    int16_t acc;               // 当前你飞行器的加速度值
    int16_t acc_error;         // 当前加速度减去重力加速度则为上下移动的加速度
    static int16_t acc_offset; // 重力加速度值
    static float thr_hold = 0; // 进入高度时记录当前油门值
    static uint8_t set_high = 0, High_breaktime;

    if (mini.flow_High < 400)//判断是否激光高度
    {
        Control_high = mini.flow_High; // 更新激光高度
    }
    else
    {
        Control_high = FlightData.High.bara_height; // 更新气压高度
    }

    //----------------------------------------------
    { // 获取垂直速度数据

        acc = (int16_t)GetAccz(); // 获取Z轴ACC

        if (!ALL_flag.unlock) // 取得静态ACC值
        {
            acc_offset = acc;
        }
        acc_error = acc - acc_offset;

        { // 此处做一个速度与高度的互补滤波
            static float last_high;
            pidHeightRate.measured = (pidHeightRate.measured + acc_error * dt) * 0.98f + 0.02f * (Control_high - last_high) / dt; // 速度环永远是主调，所以互补滤波关键就抓在速度环这里
            last_high = pidHeightHigh.measured = Control_high;                                                                    // 实时高度反馈给外环
        }
    }
    //----------------------------------------------紧急终止飞行
    if (ALL_flag.unlock == EMERGENT) // 意外情况，请使用遥控紧急上锁，飞控就可以在任何情况下紧急中止飞行，锁定飞行器，退出PID控制
        status = EXIT_255;
    //----------------------------------------------控制
    switch (status)
    {
    case WAITING_1: // 检测定高
        if (ALL_flag.height_lock && ALL_flag.unlock)//判断是否定高且解锁
        {
            LED.status = DANGEROURS;
            status = WAITING_2;
            thr_hold = 0; // 清除记录定高时的油门
        }
        break;
    case WAITING_2:                   // 定高前准备
        thr_hold = Remote.thr - 1000; // 记录定高时的油门
        set_high = 0;
        pidHeightHigh.desired = HIGH_START; // 期望高度 设定值
        status = PROCESS_31;
        break;

    case PROCESS_31: // 进入定高

        if (((Remote.thr - 1000) > (thr_hold + 10)) || ((Remote.thr - 1000) < (thr_hold - 10))) // 拨动遥杆只进行水平速度控制
        {
            if (High_breaktime++ > 20)
            {
                thr_hold = Remote.thr - 1000; // 记录定高时的油门
                High_breaktime = 0;
                set_high = 0;
            }
        }
        else
        {
        }
        if (set_high == 0) // 如果刚退出调高
        {
            set_high = 1;
            pidHeightHigh.desired = Control_high; // 记录高度当做当前定高高度
        }
        pidUpdate(&pidHeightHigh, dt);             // 调用PID处理函数来处理外环	高度PID
        pidHeightRate.desired = pidHeightHigh.out; // 高度环输出高度速度设定值

        pidUpdate(&pidHeightRate, dt); // 再调用高度速度内环PID

        //		  pidHeightRate.out += Remote.thr -1000;//加入悬停时的油门

        if (!ALL_flag.height_lock) // 退出定高
        {
            LED.status = AlwaysOn;
            status = EXIT_255;
        }
        break;
    case EXIT_255:                  // 退出定高
        pidRest(&pPidObject[6], 1); // 清除当前的定高输出值
        status = WAITING_1;         // 回到等待进入定高
        break;
    default:
        status = WAITING_1;
        break;
    }
}

void Mode_Controler(float dt)
{
    const float roll_pitch_ratio = 0.04f; // 姿态模式下 俯仰角/横滚角控制比例

    //		if(ALL_flag.unlock == 1)  //判断解锁
    {
        if (Remote.AUX2 < 1700) // 如果大于1700 则进入定点模式显示
        {
            Command.FlightMode = HEIGHT;
            ALL_flag.height_lock = 1;
            Flow_mode_two(); // 遥控-光流控制姿
            set_flag = 0x21; // OLED定高定点模式显示
        }
        else // 姿态模式
        {
            Command.FlightMode = NORMOL;
            ALL_flag.height_lock = 0;
            set_flag = 0x00; // OLED姿态模式显示

            pidPitch.desired = -(Remote.pitch - 1500) * roll_pitch_ratio; // 摇杆控制 俯仰角
            pidRoll.desired = -(Remote.roll - 1500) * roll_pitch_ratio;   // 摇杆控制 横滚角
        }
    }
}

u32 altHold_Pos_Save = 0, Pos_breaktime = 0, Pos_break_save = 0;

///////////////////////// 遥控-光流控制姿态////////////////////////////////////////////////
void Flow_mode_two(void)
{

    const float roll_pitch_ratio = 8.00f;

    if ((mini.ok == 1) && (Control_high > 20) && (Control_high < 400)) // 判断是否存在光流  高度高于20CM 光流才可以定点
    {
        Flow_SpeedPid_x.desired = (-(Remote.pitch - 1500) * 0.06f) * roll_pitch_ratio; // 直接控制速度 并且关掉外环计算
        Flow_SpeedPid_y.desired = (-(Remote.roll - 1500) * 0.06f) * roll_pitch_ratio;

        pidPitch.desired = LIMIT(Flow_SpeedPid_x.out * 0.1, -15, 15); // 姿态外环期望值
        pidRoll.desired = LIMIT(Flow_SpeedPid_y.out * 0.1, -15, 15);  // 姿态外环期望值
    }
    else
    {
        pidPitch.desired = -(Remote.pitch - 1500) * 0.04f;
        pidRoll.desired = -(Remote.roll - 1500) * 0.04f;
    }
}

/**************************************************************
 * //位置定点控制器  光流
 * @param[in]
 * @param[out]
 * @return
 ***************************************************************/
void Flow_Pos_Controler(float dt) // 位置定点控制器  光流
{
    const uint16_t DEADBAND = 150;                                     // 摇杆死区
    if ((mini.ok == 1) && (Control_high > 20) && (Control_high < 400)) // 判断是否存在光流  高度高于20CM 光流才可以定点
    {

        // 方向摇杆回中
        if ((Remote.pitch > (1500 + DEADBAND)) || (Remote.pitch < (1500 - DEADBAND)) || (Remote.roll > (1500 + DEADBAND)) || (Remote.roll < (1500 - DEADBAND))) // 拨动遥杆只进行水平速度控制
        {
            mini.flow_x_i = 0;    // 清除光流数据
            mini.flow_y_i = 0;    // 清除光流数据
            altHold_Pos_Save = 1; // 记录位置标志重新打开
            Pos_break_save = 0;
        }
        else // 拨动摇杆的时候
        {
            if (altHold_Pos_Save == 1) // 记录位置 一次
            {
                altHold_Pos_Save = 0;                     // 关闭记录位置标志
                Pos_break_save = 1;                       // 打开补偿刹车缓冲标志
                Flow_PosPid_y.desired = pixel_flow.loc_y; // 记录位置 //刷新位置期望
                Flow_PosPid_x.desired = pixel_flow.loc_x; // 记录位置	//刷新位置期望
            }
            // 外环位置控制
            Flow_PosPid_y.measured = pixel_flow.loc_y; // 实时位置反馈
            pidUpdate(&Flow_PosPid_y, dt);             // 位置运算PID
            Flow_PosPid_x.measured = pixel_flow.loc_x; // 实时位置反馈
            pidUpdate(&Flow_PosPid_x, dt);             // 位置运算PID
            // 内环期望
            Flow_SpeedPid_y.desired = LIMIT(Flow_PosPid_y.out, -1000, 1000); // 位置PID输出给速度期望
            Flow_SpeedPid_x.desired = LIMIT(Flow_PosPid_x.out, -1000, 1000); // 位置PID输出给速度期望
        }
        // 内环
        Flow_SpeedPid_y.measured = pixel_flow.loc_y; // 速度反馈
        pidUpdate(&Flow_SpeedPid_y, dt);             // 速度运算
        Flow_SpeedPid_x.measured = pixel_flow.loc_x; // 速度反馈
        pidUpdate(&Flow_SpeedPid_x, dt);             // 速度运算

        if (Pos_break_save == 1) // 定点之后增加补偿刹车缓冲  拨动摇杆之后
        {
            Flow_PosPid_y.measured = pixel_flow.loc_y; // 位置刷新
            Flow_PosPid_x.measured = pixel_flow.loc_x; // 位置刷新

            mini.flow_x_i = 0;       // 光流复位
            mini.flow_y_i = 0;       // 光流复位
            Flow_SpeedPid_x.out = 0; // 抵消惯性
            Flow_SpeedPid_y.out = 0; // 抵消惯性
            if ((Pos_breaktime++ > 100))
            {
                Pos_breaktime = 0;
                Pos_break_save = 0;
            }
        }
    }
    else
    {
        mini.flow_x_i = 0;       // 光流复位
        mini.flow_y_i = 0;       // 光流复位
        Flow_SpeedPid_x.out = 0; // 抵消惯性
        Flow_SpeedPid_y.out = 0; // 抵消惯性
    }
}

/**************************************************************
 * 姿态控制
 * @param[in]
 * @param[out]
 * @return
 ***************************************************************/
void FlightPidControl(float dt)
{
    volatile static uint8_t status = WAITING_1; // 状态机状态

    switch (status)
    {
    case WAITING_1: // 等待解锁
        if (ALL_flag.unlock)
        {
            status = READY_11;
        }
        break;
    case READY_11:              // 准备进入控制
        pidRest(pPidObject, 8); // 复位PID参数，防止上次遗留的数据影响本次控制

        Angle.yaw = pidYaw.desired = pidYaw.measured = 0; // 锁定偏航角

        status = PROCESS_31;

        break;
    case PROCESS_31:  // 正式进入控制
        // 实际测量的角速度值=6050的角速度值*校准系数Gyro_G
        pidRateX.measured = MPU6050.gyroX * Gyro_G; // 内环测量值 角度/秒
        pidRateY.measured = MPU6050.gyroY * Gyro_G; // 内环测量值 角度/秒
        pidRateZ.measured = MPU6050.gyroZ * Gyro_G; // 内环测量值 角度/秒

        // 实际测量的角度值
        pidRoll.measured = Angle.roll;   // 外环测量值 单位：角度
        pidPitch.measured = Angle.pitch; // 外环测量值 单位：角度
        pidYaw.measured = Angle.yaw;     // 外环测量值 单位：角度

        pidUpdate(&pidRoll, dt);        // 调用PID处理函数来处理外环 横滚角PID
        pidRateX.desired = pidRoll.out; // 将外环的PID输出作为内环PID的期望值即为串级PID
        pidUpdate(&pidRateX, dt);       // 再调用内环

        pidUpdate(&pidPitch, dt); // 调用PID处理函数来处理外环 俯仰角PID
        pidRateY.desired = pidPitch.out;
        pidUpdate(&pidRateY, dt); // 再调用内环

        CascadePID(&pidRateZ, &pidYaw, dt); // 也可以直接调用串级PID函数来处理
        break;
    case EXIT_255:              // 退出控制
        pidRest(pPidObject, 8); // 复位PID参数
        status = WAITING_1;     // 返回等待解锁
        break;
    default:
        status = EXIT_255;
        break;
    }
    if (ALL_flag.unlock == EMERGENT) // 意外情况，请使用遥控紧急上锁，飞控就可以在任何情况下紧急中止飞行，锁定飞行器，退出PID控制
        status = EXIT_255;
}

int16_t motor[4];
#define MOTOR1 motor[0]
#define MOTOR2 motor[1]
#define MOTOR3 motor[2]
#define MOTOR4 motor[3]

void MotorControl(void) // 电机控制
{
    volatile static uint8_t status = WAITING_1; // 状态机状态

    if (ALL_flag.unlock == EMERGENT) // 遥控紧急上锁，飞控就可以在任何情况下紧急中止飞行，锁定飞行器，退出PID控制
        status = EXIT_255;
    switch (status)
    {
    case WAITING_1:                            // 等待解锁
        MOTOR1 = MOTOR2 = MOTOR3 = MOTOR4 = 0; // 如果锁定，则电机输出都为0
        if (ALL_flag.unlock)
        {
            status = WAITING_2;
        }
    case WAITING_2: // 解锁完成后判断使用者是否开始拨动遥杆进行飞行控制
        if (Remote.thr > 1100)
        {
            status = PROCESS_31; // 控制状态
        }
        break;
    case PROCESS_31: {
        int16_t thr_temp;

        if (ALL_flag.height_lock) // 定高模式下 油门遥杆作为调整高度使用
        {
            thr_temp = pidHeightRate.out + (Remote.thr - 1000); // 输出给电机的是定高输出值
        }
        else // 正常飞行状态，油门正常使用
        {
            thr_temp = Remote.thr - 1000; // 输出给电机的是油门输出值
        }

        if (Remote.thr < 1020) // 油门太低了，则限制输出  不然飞机乱转
        {
            MOTOR1 = MOTOR2 = MOTOR3 = MOTOR4 = 0;
            break;
        }
        MOTOR1 = MOTOR2 = MOTOR3 = MOTOR4 = LIMIT(thr_temp, 0, 900); // 留100给姿态控制

        MOTOR1 += +pidRateX.out - pidRateY.out - pidRateZ.out; //; 姿态输出分配给各个电机的控制量
        MOTOR2 += +pidRateX.out + pidRateY.out + pidRateZ.out; //;
        MOTOR3 += -pidRateX.out + pidRateY.out - pidRateZ.out;
        MOTOR4 += -pidRateX.out - pidRateY.out + pidRateZ.out; //;
    }
    break;
    case EXIT_255:
        MOTOR1 = MOTOR2 = MOTOR3 = MOTOR4 = 0; // 如果锁定，则电机输出都为0
        status = WAITING_1; // 返回等待解锁
        break;
    default:

        break;
    }

    //	TIM2->CCR1 = LIMIT(MOTOR1,0,1000);  //更新PWM1
    //	TIM2->CCR2 = LIMIT(MOTOR2,0,1000);  //更新PWM2
    //	TIM3->CCR1 = LIMIT(MOTOR3,0,1000);  //更新PWM3
    //	TIM3->CCR2 = LIMIT(MOTOR4,0,1000);  //更新PWM4
    ////	TIM2->CCR3 = LIMIT(MOTOR3,0,1000);  //更新PWM3
    ////	TIM2->CCR4 = LIMIT(MOTOR4,0,1000);  //更新PWM4

#if (FLY_TYPE == 1 || FLY_TYPE == 2) // 1轴/2轴飞行器

    PWM0 = LIMIT(MOTOR1, 0, 1000); // 更新PWM1
    PWM1 = LIMIT(MOTOR2, 0, 1000); // 更新PWM2
    PWM2 = LIMIT(MOTOR3, 0, 1000); // 更新PWM3
    PWM3 = LIMIT(MOTOR4, 0, 1000); // 更新PWM4

#elif (FLY_TYPE >= 3)// 3轴/4轴飞行器

    PWM0 = 1000 + LIMIT(MOTOR1, 0, 1000); // 更新PWM1
    PWM1 = 1000 + LIMIT(MOTOR2, 0, 1000); // 更新PWM2
    PWM2 = 1000 + LIMIT(MOTOR3, 0, 1000); // 更新PWM3] ;
    PWM3 = 1000 + LIMIT(MOTOR4, 0, 1000); // 更新PWM4

#else
#error Please define FLY_TYPE!

#endif
}

/************************************END OF FILE********************************************/
