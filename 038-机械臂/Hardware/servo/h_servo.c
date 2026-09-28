#include "./servo/h_servo.h"

/* 总线舵机回中指令需经 USART3 下发。zx_uart_send_str 在 app_servo.c 中定义,
   此处在调用前前向声明, 避免 Keil 报 "implicit declaration" / #159 不兼容错误。 */
void zx_uart_send_str(u8* str);

/* ============================================================================
 *  舵机驱动 h_servo.c —— PWM舵机(0~5号, 共6路)的输出与运动控制
 * ----------------------------------------------------------------------------
 *  duoji_doing[] 是每个舵机的"运动结构体", 4个字段决定平滑运动(结构体定义在h_servo.h):
 *    cur : 当前脉冲宽度(us), 直接决定舵机角度(TIM2中断把它写进ARR)
 *    aim : 目标脉冲宽度(us), 想转到的位置
 *    inc : 每20ms步进增量(= (aim-cur)/(time/20)), 不为0时舵机在慢慢扫
 *    time: 期望执行时间(ms)
 *  关键技巧: 改角度不是直接改cur, 而是改aim+inc, TIM2每次中断把cur往aim挪一点 → 平滑转动
 *  (PWM舵机共6路: SERVO0~5 = PB3/PB8/PB9/PB6/PB7/PB4; 总线舵机6个走USART3, 不在这里)
 *  注意: DJ_NUM=8 仅用于 TIM2 分时8个时隙, 其中6路有物理引脚, 2路留空。
 * ========================================================================== */

servo_t duoji_doing[DJ_NUM];//舵机数量

/* 舵机gpio初始化 */
void servo_init(void) {
    u8 i;
    RCC_APB2PeriphClockCmd(SERVO0_GPIO_CLK | SERVO1_GPIO_CLK | SERVO2_GPIO_CLK |
    SERVO3_GPIO_CLK | SERVO4_GPIO_CLK | SERVO5_GPIO_CLK, ENABLE); // 使能舵机端口时钟
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = SERVO0_PIN;        // 配置引脚
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; // IO翻转50MHZ
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;  // 推挽输出
    GPIO_Init(SERVO0_GPIO_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = SERVO1_PIN;
    GPIO_Init(SERVO1_GPIO_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = SERVO2_PIN;
    GPIO_Init(SERVO2_GPIO_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = SERVO3_PIN;
    GPIO_Init(SERVO3_GPIO_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = SERVO4_PIN;
    GPIO_Init(SERVO4_GPIO_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = SERVO5_PIN;
    GPIO_Init(SERVO5_GPIO_PORT, &GPIO_InitStructure);

    /* 给每个舵机进行初始化赋值 */
    for (i = 0; i < DJ_NUM; i++)
    {
        duoji_doing[i].aim = 1500;  //执行目标
        duoji_doing[i].cur = 1500;  //当前值
        duoji_doing[i].inc = 0;     //增量
        duoji_doing[i].time = 5000; //执行时间
    }

    /* 这里广播 #255 让全部总线舵机平滑回到中位1500, 使 $RST! 软复位后机械臂整体回中。 */
    zx_uart_send_str((u8*)"#255P1500T2000!");
}

/* 设置舵机引脚电平，参数 index 舵机引脚索引，level 舵机引脚电平 */
void servo_pin_set(u8 index, BitAction level) {
    switch (index) {
        case 0:SERVO0_PIN_SET(level); break;
        case 1:SERVO1_PIN_SET(level); break;
        case 2:SERVO2_PIN_SET(level); break;
        case 3:SERVO3_PIN_SET(level); break;
        case 4:SERVO4_PIN_SET(level); break;
        case 5:SERVO5_PIN_SET(level); break;
        default:break;
    }
}

/* 设置舵机控制参数函数，参数 index 舵机编号 aim 执行目标 time 执行时间
(如果 aim 执行目标==0，视为舵机停止) */
void duoji_doing_set(u8 index, int aim, int time) {
    /* 限制输入值大小 */
    if (index >= DJ_NUM)
    {
        return;
    }
    /* 执行目标 */
    if (aim == 0) {
        duoji_doing[index].inc = 0; //增量
        duoji_doing[index].aim = duoji_doing[index].cur;//当前值赋值给执行目标
        return;
    }else if (aim > 2490) {
        aim = 2490;   // 上限钳位: 超过2500会让 TIM2下半拍 2500-cur 变负→ARR溢出, 舵机抽风
    }else if (aim < 510) {
        aim = 510;    // 下限钳位: 小于500则高脉冲过窄, 舵机可能不识别
    }

    /* 执行时间 */
    if (time > 10000)
    {
        time = 10000;
    }
    /* 当前值 */
    if (duoji_doing[index].cur == aim) {
        aim = aim + 0.0077; //微调
    }
    if (time < 20) {/*执行时间太短，舵机直接以最快速度运动*/
        duoji_doing[index].aim = aim;
        duoji_doing[index].cur = aim;
        duoji_doing[index].inc = 0;
    }else {
        duoji_doing[index].aim = aim;
        duoji_doing[index].time = time;
        duoji_doing[index].inc = (duoji_doing[index].aim - duoji_doing[index].cur) /
        (duoji_doing[index].time / 20.000);
    }
}

/* 处理绝对值 */
float abs_float(float value) {
    if (value > 0) {
        return value;
    }
    return (-value);
}

/* 设置舵机每次增加的偏移量 */
void servo_inc_offset(u8 index) {
    int aim_temp;//执行目标临时变量
    if (duoji_doing[index].inc != 0)
    {
        aim_temp = duoji_doing[index].aim;
        if (aim_temp > 2490) {
            aim_temp = 2490;
        }else if (aim_temp < 500) {
            aim_temp = 500;
        }

        /* 如果执行距离过短，直接运行到执行距离，不用增量 */
        if (abs_float(aim_temp - duoji_doing[index].cur) <=
        abs_float(duoji_doing[index].inc + duoji_doing[index].inc)) {
            duoji_doing[index].cur = aim_temp;
            duoji_doing[index].inc = 0;
        }else {
            duoji_doing[index].cur += duoji_doing[index].inc;
        }
    }
}

