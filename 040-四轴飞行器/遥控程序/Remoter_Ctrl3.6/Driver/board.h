#ifndef __BOARD_H__
#define __BOARD_H__
#include "stm32f10x.h"
#include "ANO_Drv_Uart.h"
#include "ANO_Drv_Uart3.h"
#include "ANO_Drv_SPI.h"
#include "ANO_Drv_Nrf24l01.h"
#include "ANO_Drv_LED.h"
#include "ANO_Drv_hid.h"
#include "ANO_Drv_ADC.h"
#include "ANO_DT.h"

//#define GPIO_Remap_SWJ_JTAGDisable  ((uint32_t)0x00300200)  /*!< JTAG-DP Disabled and SW-DP Enabled */

#define HW_TYPE	1
#define HW_VER	3
#define BL_VER	100
#define PT_VER	400
/***************LED GPIO定义******************/
#define ANO_RCC_LED_0					RCC_APB2Periph_GPIOB
#define ANO_GPIO_LED_0				GPIOB
#define ANO_Pin_LED_0					GPIO_Pin_9
#define LED_Red_GPIO		      GPIOB
#define LED_Red_Pin		        GPIO_Pin_1

#define ANO_LED_blue_ON  		GPIO_SetBits(LED_Red_GPIO, LED_Red_Pin)
#define ANO_LED_blue_OFF 		GPIO_ResetBits(LED_Red_GPIO, LED_Red_Pin)

//#define LED_Red_On   		GPIO_ResetBits(ANO_GPIO_LED_0, ANO_Pin_LED_0)
//#define LED_Red_OFF  		GPIO_SetBits(ANO_GPIO_LED_0, ANO_Pin_LED_0)

#define LED_Red_On   				GPIO_SetBits(ANO_GPIO_LED_0, ANO_Pin_LED_0)
#define LED_Red_OFF  				GPIO_ResetBits(ANO_GPIO_LED_0, ANO_Pin_LED_0)

/*********************************************/
/***************蜂鸣器 GPIO定义******************/

//#define BEEP_P              GPIOC
//#define BEEP		            GPIO_Pin_13

#define BEEP_P              GPIOB
#define BEEP		            GPIO_Pin_10

#define BEEP_L          GPIO_ResetBits(BEEP_P, BEEP)
#define BEEP_H          GPIO_SetBits(BEEP_P, BEEP)



/*********************************************/
/***************I2C GPIO定义******************/
#define ANO_GPIO_I2C	GPIOC
#define I2C_Pin_SCL		GPIO_Pin_14
#define I2C_Pin_SDA		GPIO_Pin_15
#define ANO_RCC_I2C		RCC_APB2Periph_GPIOB
/*********************************************/
/***************UART1 GPIO定义******************/
#define ANO_RCC_UART1			RCC_APB2Periph_GPIOA
#define ANO_GPIO_UART1		GPIOA
#define ANO_UART1_Pin_TX	GPIO_Pin_9
#define ANO_UART1_Pin_RX	GPIO_Pin_10
/*********************************************/
/***************SPI GPIO定义******************/
#define ANO_GPIO_SPI		GPIOA
#define RCC_GPIO_SPI		RCC_APB2Periph_GPIOA
#define SPI_Pin_SCK			GPIO_Pin_5
#define SPI_Pin_MISO		GPIO_Pin_6
#define SPI_Pin_MOSI		GPIO_Pin_7

#define NRF_CE_GPIO		GPIOA		
#define NRF_CE_Pin		GPIO_Pin_15	
#define NRF_CSN_GPIO	GPIOA		
#define NRF_CSN_Pin		GPIO_Pin_4	
#define NRF_IRQ_GPIO	GPIOB
#define NRF_IRQ_Pin		GPIO_Pin_1

/*********************************************/
/***************硬件中断优先级******************/
#define NVIC_UART_P	5
#define NVIC_UART_S	1
/***********************************************/
extern uint32_t SysTick_count;

void Delay(vu32 nCount);
void cycleCounterInit(void);
void SysTick_IRQ(void);

#endif 
