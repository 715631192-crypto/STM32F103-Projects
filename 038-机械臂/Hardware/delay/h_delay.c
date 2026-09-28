#include "main.h"

void Delay_ns(u32 xns)
{
	while (xns--)
		;
	return;
}
void Delay_us(u32 xus)
{
	// 基于SysTick计数器VAL的精确us级延时(72MHz下每us=72个时钟)
	// SysTick为递减计数器，每1ms自动重载，需处理跨越重载点的情况
	u32 ticks = xus * (SystemCoreClock / 1000000u);
	u32 start = SysTick->VAL;
	while (1)
	{
		u32 cur = SysTick->VAL;
		u32 elapsed;
		if (cur <= start)
			elapsed = start - cur;                    // 未跨越1ms重载点
		else
			elapsed = (SysTick->LOAD + 1u - start) + cur; // 跨越了重载点
		if (elapsed >= ticks)
			break;
	}
}

/**
 * @brief  毫秒级延时
 * @param  xms 延时时长
 * @retval 无
 */
void Delay_ms(u32 xms)
{
	u32 systick_temp = SysTick_get_ms();
	while (xms > (SysTick_get_ms() - systick_temp))	
	{
		if(systick_temp>(SysTick_get_ms()))
		{
			xms = xms - (0xffffffff - systick_temp); // 还剩余未计数
			systick_temp = 0;
		}
	}
}

/**
 * @brief  秒级延时
 * @param  xs 延时时长，范围：0~4294967295
 * @retval 无
 */
void Delay_s(u32 xs)
{
	while (xs--)
	{
		Delay_ms(1000);
	}
}
