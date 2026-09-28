/* ============================================================================
 *  六轴机械臂主控 main.c 
 * ----------------------------------------------------------------------------
 * 
 *    电脑串口助手 / PS2手柄按键
 *            │
 *            ▼
 *    uart_receive_buf(全局接收缓冲) + uart1_mode(指令类型) + uart1_get_ok(收完标志)
 *            │
 *            ▼
 *    app_usart_run() 按 uart1_mode 分发:
 *        mode1=$命令 → parse_cmd()      系统命令(复位/停止/动作组/蜂鸣/开机动作$BOOT:x!)
 *        mode2=#舵机 → parse_action()   单舵机移动, 同时原样转发到总线舵机口USART3
 *        mode3={...} → parse_action()   多路舵机(与mode2共用解析)
 *        mode4=<...> → save_action()    动作组存进W25Q64 Flash
 *        mode5=S005,90D → parse_angle()  角度模式: 0~270度线性映射 P500~2500(新增)
 *            │
 *            ▼
 *    PWM舵机(0~5号): duoji_doing[] → TIM2中断分时输出波形(PB3/6/8/9)
 *    总线舵机(0~5号): USART3串口(PB10) → 舵机自身解析 #IDPposTtime!
 * ========================================================================== */
#include "main.h"
int main(void)
{
    SWJ_GPIO_Init();   // ① 释放SWJ/JTAG引脚(PA13/14/15,PB3/4)成普通GPIO —— 不释放PS2和舵机脚都用不了
    rcc_Init();        // ② 时钟树初始化(系统72MHz)
    SysTick_Init();    // ③ 系统滴答定时器: 提供毫秒延时与计时(按键扫描/PS2每50ms轮询都靠它)
    app_gpio_init();   // ④ LED/蜂鸣器/板载按键的GPIO
    app_setup_start(); // ⑤ 初始化系统, 闪烁LED3次, 1s后进入正常模式
    spi_flash_init();  // ⑥ W25Q64 Flash初始化(存动作组/偏差值); 注意: 它占用PB13, 末尾已恢复成LED
    load_eeprom();     // ⑥+ 上电把 eeprom_info(开机动作选择等)从Flash读回RAM
    app_usart_init();  // ⑦ 串口1(PA9/PA10, 回显/上位机) + 串口3(PB10/PB11, 总线舵机) 均115200
    servo_init();      // ⑧ PWM舵机GPIO(PB3/6/8/9等) + duoji_doing[] 初始中位1500
    TIM2_init(20000,72);//⑨ TIM2时基: 72MHz/72分频=1MHz→1us/tick, 周期值20000即20ms; 用于分时PWM
    app_ps2_init();    // ⑩ PS2手柄GPIO + 进入红灯(模拟)模式初始化序列
    boot_action_run(); // ⑪ 按 $BOOT:x! 选的开机动作回放(挥手/伸展/自定义), 无则保持中位
    while (1)
    {
        app_led_run();     // LED状态闪烁
        app_key_run();     // 板载按键
        app_usart_run();   // 串口指令分发(看 uart1_mode) 
        loop_action();     // 动作组循环执行器(对应 $DGT 多次执行)
        app_ps2_run();     // PS2手柄读取→按键表→注入uart_receive_buf(复用上面同一管道)
    }
    
}

// SWJ引脚配置
void SWJ_GPIO_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);
}
