#include "stm32f10x.h"
#include "OLED.h"
#include "Delay.h"
#include "USART.h"
#include <stdio.h>
#include "DHT11.h"
#include "ESP8266.h"
#include "onenet.h"
#include "LED.h"
#include "MqttKit.h"
#include "KEY.h"
#include "BUZZER.h"
//储存dht11温度、湿度
uint8_t temp, humi;
//控制50次发送上传一次数据
uint8_t timeCount = 0;

//初始化函数
void Main_Init(void)
{
    //中断优先级分组                                                                                                                                                                                                                                                              
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	OLED_Init();
	LED_Init();
	BUZZER_Init();
	KEY_Init();
	Usart1_Init(115200);
	Usart2_Init(115200);
	while (DHT11_Init() == 1)
	{
		UsartPrintf(USART_DEBUG, "DHT11_Init failed\r\n");
		Delay_ms(500);
	}
	ESP8266_Init();
	UsartPrintf(USART_DEBUG, "Connect MQTT Server Start\r\n");
	while (ESP8266_SendCmd(ESP8266_ONENET_INFO, "CONNECT"))//发送TCP链接指令
	{

		Delay_ms(500);
	}
	UsartPrintf(USART_DEBUG, "Connect MQTT Server Success\r\n");
	while (OneNet_DevLink())//与onenet创建连接
	{
		Delay_ms(500);
	}

	OneNET_Subscribe();//订阅
}

int main(void)
{
	uint8_t last_led_state = 0;//记录上次灯的状态
	uint8_t last_buzzer_state = 0;

	uint8_t *data_ptr = NULL;

	Main_Init();
	OLED_Clear();
	OLED_ShowChinese(1, 1, 0);//温
	OLED_ShowChinese(1, 3, 1);//度
	OLED_ShowChinese(2, 1, 3);//湿
	OLED_ShowChinese(2, 3, 1);
	OLED_ShowString(3, 1, "LED");
	OLED_ShowChinese(3, 4, 5);//灯
	OLED_ShowChinese(3, 8, led_state ? 6 : 7);//亮：灭
	OLED_ShowString(4, 1, "BUZZER");
	OLED_ShowString(4, 8, buzzer_state ? "ON" : "OFF");

	while (1)
	{
		KEY_Scan();

		DHT11_Read_Data(&temp, &humi);
		OLED_ShowNum(1, 10, temp, 2);
		OLED_ShowNum(2, 10, humi, 2);

		if (led_state != last_led_state)
		{
			OLED_ShowChinese(3, 8, led_state ? 6 : 7);
			last_led_state = led_state;
		}

		if (buzzer_state != last_buzzer_state)
		{
			OLED_ShowString(4, 8, buzzer_state ? "ON" : "OFF");
			last_buzzer_state = buzzer_state;
		}

		Delay_ms(50);

		if (timeCount++ >= 50)
		{
			ESP8266_Clear();
			UsartPrintf(USART_DEBUG, "OneNet_SendData\r\n");
			OneNet_SendData();
			timeCount = 0;
		}
		data_ptr = ESP8266_GetIPD(0);
		if (data_ptr)
		{
			OneNet_RevPro(data_ptr);
		}
	}
}
