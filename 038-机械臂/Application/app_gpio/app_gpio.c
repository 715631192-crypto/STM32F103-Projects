#include "./app_gpio/app_gpio.h"
//初始化app_gpio
void app_gpio_init(void)
{
    led_init();
    beep_init();
    key_init();
    LED_OFF();
    BEEP_OFF();
}
//app_led运行每1s切换一次led状态
void app_led_run(void)
{
    static uint32_t time_count = 0;
    if ((SysTick_get_ms() - time_count) < 1000)
    {
        return;
    }
    time_count = SysTick_get_ms();
    LED_TOGGLE();
}
//app_setup_start设置开始
void app_setup_start(void)
{
    for (int i = 0; i < 3; i++)
    {
        BEEP_ON();
        LED_ON();
        Delay_ms(100);
        BEEP_OFF();
        LED_OFF();
        Delay_ms(100);
    }
}
//app_key_run扫描按键状态
void app_key_run(void)
{
  
    if (key1_pressing != 0)
    {
        if (key1_pressing == 1)
        {
            app_setup_start();
            key1_pressing = 0;
        }
        else if(key1_pressing == 2)
        {
            LED_ON();
        }
    }
    if (key2_pressing != 0)
    {
        if (key2_pressing == 1)
        {
            app_setup_start();
            key2_pressing = 0;
        }
        else if(key2_pressing == 2)
        {
            LED_ON();
        }
    }
}
