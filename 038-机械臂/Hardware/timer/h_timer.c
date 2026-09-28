#include "./timer/h_timer.h"

static uint32_t systick_ms=0;
//初始化SysTick定时器
void SysTick_Init(void)
{
    //初始化并启动嘀嗒计时器和中断
    SysTick_Config(SystemCoreClock/1000);//1ms中断
}

//SysTick中断服务函数
void SysTick_Handler(void)
{
    systick_ms++;
    static uint32_t key_scan_time = 0;
    key_scan_time++;
    if (key_scan_time >= 10)
    {
        key_scan();
        key_scan_time = 0;
    }   
}

//获取滴答时钟数值
u32 SysTick_get_ms(void)
{
    return systick_ms;
}

/**
* @函数描述: 初始化TIM2，用于生成pwm控制舵机
* @param {u16} arr 计数器自动重载值
* @param {u16} psc 预分频器
* @return {}
*/
void TIM2_init(u16 arr, u16 psc)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // 时钟 TIM2 使能
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    // 定时器 TIM2 初始化
    TIM_TimeBaseStructure.TIM_Period = arr-1;              // 设置自动重载寄存器周期的值
    TIM_TimeBaseStructure.TIM_Prescaler = psc-1;           // 设置时钟频率除数的预分频值
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; // 输入捕获分频：不分频
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; // 向上计数
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);      // 初始化 TIM2

    TIM_ARRPreloadConfig(TIM2, DISABLE);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE); // 允许更新中断

    // 中断优先级 NVIC 设置
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;             // TIM2 中断
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;    // 先占优先级 0 级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;           // 从优先级 3 级
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;             // IRQ 通道被使能
    NVIC_Init(&NVIC_InitStructure);                              // 初始化 NVIC 寄存器

    TIM_Cmd(TIM2, ENABLE); // 使能 TIM2
}

/* ============================================================================
 *  TIM2中断 —— 用"1个定时器"分时驱动8路PWM舵机
 * ----------------------------------------------------------------------------
 *  核心思想: 时间切成8个互斥时隙, 每个2.5ms(2500us), 任意时刻只动一个舵机脚。
 *    TIM2 = 1us/tick(72MHz/72), 所以 ARR = N 表示"数N个tick(即N us)后触发下次中断"。
 *    flag==0: 把当前舵机脚拉高, ARR=cur → 高电平持续 cur us (= 舵机角度脉冲)
 *    flag==1: 把当前舵机脚拉低, ARR=2500-cur → 填满本时隙剩余时间, 然后切下一个舵机
 *    单个舵机一拍 = cur + (2500-cur) = 2500us 闭合; 8拍 = 20ms = 标准舵机周期。
 *  于是每个舵机每20ms被照顾一次, 收到一条 cur us 宽的高脉冲 → 标准20ms周期PWM。
 *  注意: ARR在这里不是20ms周期, 而是"下一个半拍的时长"; 20ms是8×2.5ms累加出来的。
 * ========================================================================== */
/* 定时器2中断函数，输出舵机控制波形 */
void TIM2_IRQHandler(void) //每2.5ms触发一次(时隙切换), 8个时隙拼成20ms
{
    static u8 flag = 0;
    static u8 duoji_index1 = 0;
    int temp;

    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) // 检查 TIM2 更新中断发生与否
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update); // 清除 TIM2 更新中断标志

        /* 下标轮询: 0~DJ_NUM-1 循环, 每个舵机占一个时隙 */
        if (duoji_index1 == DJ_NUM)
        {
            duoji_index1 = 0;
        }

        if (flag == 0)
        {
            // 上半拍: 拉高, ARR=cur → 高电平宽度 = cur us(舵机角度由它决定)
            TIM2->ARR = (unsigned int)(duoji_doing[duoji_index1].cur);
            servo_pin_set(duoji_index1, Bit_SET);
            servo_inc_offset(duoji_index1); // 把cur往aim挪一点(平滑转动的关键)
        }
        else
        {
            // 下半拍: 拉低, ARR=2500-cur → 补齐2.5ms时隙, 然后切到下一个舵机
            temp = 2500 - (unsigned int)(duoji_doing[duoji_index1].cur);
            TIM2->ARR = temp;
            servo_pin_set(duoji_index1, Bit_RESET);
            duoji_index1++;
        }
        flag = !flag;
    }
}



