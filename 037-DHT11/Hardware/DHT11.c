#include "stm32f10x.h" // Device header
#include "DHT11.h"
#include "Delay.h"

// 设置IO为输入模式
void DHT11_IO_IN(void)
{
    GPIOA->CRL &= 0xfffffff0;
    GPIOA->CRL |= 8;
}

// 设置IO为输出模式
void DHT11_IO_OUT(void)
{
    GPIOA->CRL &= 0xfffffff0;
    GPIOA->CRL |= 3;
}

// 设置引脚电平
void DHT11_DQ_OUT(uint8_t bit)
{
    GPIO_WriteBit(GPIOA, GPIO_Pin_0, (BitAction)bit);
}

// 读引脚电平
uint8_t DHT11_DQ_IN(void)
{
    return GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0);
}

// 主机起始条件
void DHT11_Rst(void)
{
    DHT11_IO_OUT();//输出模式
    DHT11_DQ_OUT(0);//输出低电平
    Delay_ms(20);//>=18ms
    DHT11_DQ_OUT(1);//输出高电平
    Delay_us(30);//主机拉高20——40us
}

// 等待DHT11应答 1 存在  0 没应答
uint8_t DHT11_Check(void)
{
    uint8_t retry = 0;
    DHT11_IO_IN();//输入模式
    while ((DHT11_DQ_IN() == SET) && retry < 100)//拉低80us
    {
        retry++;
        Delay_us(1);
    }
    if (retry >= 100)
    {
        return 1;
    }
    retry = 0;
    while ((DHT11_DQ_IN() == RESET) && retry < 100)//拉高80us
    {
        retry++;
        Delay_us(1);
    }
    if (retry > 100)
    {
        return 1;
    }
    return 0;
}

// 读取一个bit
uint8_t DHT11_Read_Bit(void)
{
    uint8_t retry = 0;
    while ((DHT11_DQ_IN() == SET) && retry < 100)//拉低50us
    {
        retry++;
        Delay_us(1);
    }
    retry = 0;
    while ((DHT11_DQ_IN() == RESET) && retry < 100)//拉高
    {
        retry++;
        Delay_us(1);
    }
    Delay_us(40);//等待40us
    return DHT11_DQ_IN();
}

// 读取一个字节
uint8_t DHT11_Read_Byte(void)
{
    uint8_t data = 0x00;
    for (int i = 0; i < 8; i++)
    {
       data |= (DHT11_Read_Bit() << (7 - i));
    }
    return data;
}

// 读取40位存入数组
// 1 失败    0  成功
uint8_t DHT11_Read_Data(uint8_t *temp, uint8_t *humi)
{
    uint8_t buf[5];
    DHT11_Rst();
    if (DHT11_Check() == 0)
    {
        for (int i = 0; i < 5; i++)
        {
            buf[i] = DHT11_Read_Byte();
        }

        if ((buf[0] + buf[1] + buf[2] + buf[3]) == buf[4])
        {
            *temp = buf[2];
            *humi = buf[0];
        }
        else
        {
            return 1;
        }
    }
    return 0;
}

//初始化
uint8_t DHT11_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
     // 初始化GPIOA，并设置为通用推挽输出
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStruct);
    GPIO_SetBits(GPIOA,GPIO_Pin_0);
    DHT11_Rst();
    return DHT11_Check();
}
