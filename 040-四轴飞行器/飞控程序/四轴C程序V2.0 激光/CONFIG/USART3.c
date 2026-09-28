

#include "usart3.h"
#include "misc.h"
#include "stdio.h"
#include "ALL_DEFINE.h"

/*
 * 函数名：USART1_Config
 * 描述  ：USART1 GPIO 配置,工作模式配置。
 * 输入  ：无
 * 输出  : 无
 * 调用  ：外部调用
 */

void USART3_Config(u32 br_num) 
{
	USART_InitTypeDef USART_InitStructure;
	USART_ClockInitTypeDef USART_ClockInitStruct;
	GPIO_InitTypeDef GPIO_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE); //开启USART3时钟
	RCC_APB2PeriphClockCmd(RCC_UART3,ENABLE);	

	/*串口GPIO端口配置*/
  /* 配置串口1 （USART3 Tx (PA.10)）*/
	GPIO_InitStructure.GPIO_Pin = UART3_Pin_TX;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIO_UART3, &GPIO_InitStructure);
  
	/* 配置串口1 USART3 Rx (PA.11)*/
  GPIO_InitStructure.GPIO_Pin = UART3_Pin_RX;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIO_UART3, &GPIO_InitStructure);
	
	
	//USART 初始化设置
	USART_InitStructure.USART_BaudRate =br_num;//串口波特率
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;//字长为8位数据格式
	USART_InitStructure.USART_StopBits = USART_StopBits_1;//一个停止位
	USART_InitStructure.USART_Parity = USART_Parity_No;//无奇偶校验位
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//无硬件数据流控制
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	//收发模式
	
	
		//配置USART3时钟
	USART_ClockInitStruct.USART_Clock = USART_Clock_Disable;  //时钟低电平活动
	USART_ClockInitStruct.USART_CPOL = USART_CPOL_Low;  //SLCK引脚上时钟输出->低电平
	USART_ClockInitStruct.USART_CPHA = USART_CPHA_2Edge;  //
	USART_ClockInitStruct.USART_LastBit = USART_LastBit_Disable; //
	
	
	USART_Init(USART3, &USART_InitStructure);
	USART_ClockInit(USART3, &USART_ClockInitStruct);
	
	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);//开启中断
	
	USART_Cmd(USART3, ENABLE); //使能串口 
	
		/* 使能串口1中断 */
	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;	//USART3  串口1全局中断 
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = NVIC_UART3_P;//抢占优先级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = NVIC_UART3_S; //子优先级
	/*IRQ通道使能*/
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	/*根据NVIC_InitStruct中指定的参数初始化外设NVIC寄存器USART3*/
	NVIC_Init(&NVIC_InitStructure);
	
	
	
}

////////////////////////////////////////////


//串口3中断程序在  ANO_DT.C 程序里面


////////////////////////////////////////////

//void ANO_UART3_DeInit(void)
//{
//	NVIC_InitTypeDef NVIC_InitStructure;
//	
//	USART_DeInit(USART3);
//	USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
//	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
//	NVIC_InitStructure.NVIC_IRQChannelCmd = DISABLE;
//	NVIC_Init(&NVIC_InitStructure);
//}



void USART3_SendByte(const int8_t *Data,uint8_t len)
{ 
	uint8_t i;
	
	for(i=0;i<len;i++)
	{
		while (!(USART3->SR & USART_FLAG_TXE));	 // while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
		USART_SendData(USART3,*(Data+i));	 
	}
}

void USART3_Send1(const int8_t Data)  
{ 

		while (!(USART3->SR & USART_FLAG_TXE));	 // while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
		USART_SendData(USART3,(Data));	 
	
}


int8_t CheckSend[7]={0xAA,0XAA,0xEF,2,0,0,0};

void USART1_setBaudRate(uint32_t baudRate)
{
	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate =  baudRate;
	USART_Init(USART1, &USART_InitStructure);
}








