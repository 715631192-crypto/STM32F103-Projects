#include "./key/h_key.h"
uint8_t key1_pressing = 0;//按键1是否按下
uint8_t key2_pressing = 0;//按键2是否按下
static uint8_t key1_flag = 0;//按键1状态
static uint8_t key2_flag = 0;//按键2状态
static uint16_t key1_press_time = 0;//按键1按下时间
static uint16_t key2_press_time = 0;//按键2按下时间
//初始化key
void key_init(void)
{
    // 使能时钟
    RCC_APB2PeriphClockCmd(KEY1_GPIO_CLK|KEY2_GPIO_CLK, ENABLE);

    // 初始化GPIOA，并设置为上拉输入
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Pin = KEY1_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Pin = KEY2_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void key_scan(void)
{
    // 扫描按键状态
    if(KEY1() == 0)
    {
        key1_press_time++;
        if(key1_flag == 0)//防止重复处理
        {//50ms消抖后，确认需要处理
           if(key1_press_time >DITHER_ELIMINATION_TIME)
            {
                key1_flag = 1;
            }
        }
        if(key1_flag == 1)
        {
            if(key1_press_time >LONG_PRESS_TIME)
            {
                key1_flag = 2;
                key1_pressing = 2;
            }
        }
    }
    else
    {
        if(key1_flag == 1)
        {
            key1_pressing = 1;
        }
        else
        {
            key1_pressing = 0;
        }
        key1_flag=0;
        key1_press_time = 0;  
    }
    if(KEY2() == 0)
    {
        key2_press_time++;
        if(key2_flag == 0)
        {
           if(key2_press_time >DITHER_ELIMINATION_TIME)
            {
                key2_flag = 1;
            }
        }
        if(key2_flag == 1)
        {
            if(key2_press_time >LONG_PRESS_TIME)
            {
                key2_flag = 2;
                key2_pressing = 2;
            }
        }
    }
    else
    {
        if(key2_flag == 1)
        {
            key2_pressing = 1;
        }
        else
        {
            key2_pressing = 0;
        }
        key2_flag=0;
        key2_press_time = 0;  
    }
}


