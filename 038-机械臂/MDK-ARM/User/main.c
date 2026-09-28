#include "stm32f10x.h"
#include "OLED.h"
#include "delay.h"
#include "LED.h"
#include "misc.h"

int ldr = 0;

int main(int argc, char const *argv[])
{

	LED_Init();
	// 让led灯跑马然后越来越快,到一定时间返回跑马,然后再回正继续

	while (1)
	{
		int delaytime = 1000; // ms

		if (ldr == 0)

		{
			for (uint16_t i = GPIO_Pin_0; i <= GPIO_Pin_5; i <<= 1)
			{
				LED_ON(i);
				delay_ms(delaytime);
				
			}
			delaytime -= 50;
			if (delaytime <= 100)
			{
				ldr = 1;
			}
		}
		if (ldr == 1)
		{
			for (uint16_t i = GPIO_Pin_5; i >= GPIO_Pin_0; i >>= 1)
			{
				LED_ON(i);
				delay_ms(delaytime);
			}
			delaytime -= 60;
            // 延时到最小值，切回正向并重置初始慢速
            if(delaytime <= 100)
            {
                ldr = 0;
                delaytime = 1000; // 恢复最慢速度重新循环
		}
	}
}
