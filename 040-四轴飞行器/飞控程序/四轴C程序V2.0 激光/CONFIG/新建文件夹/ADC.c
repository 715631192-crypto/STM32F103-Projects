//========================================================================
//	爱好者电子工作室-淘宝 https://devotee.taobao.com/
//	STM32四轴爱好者QQ群: 810149456
//	作者：小刘
//	电话:13728698082
//	邮箱:1042763631@qq.com
//	日期：2018.05.17
//	版本：V1.0
//========================================================================
//套件购买地址：https://devotee.taobao.com/
//                 爱好者电子工作室
//特此声明：
//
//         此程序只能用作学习，如用商业用途。必追究责任！
//          
//
//

#include "sys.h"
#include "ADC.h"

/***************ADC GPIO??******************/
#define RCC_GPIO_ADC	RCC_APB2Periph_GPIOB
#define GPIO_ADC			GPIOB
#define GPIO_Pin_ADC	GPIO_Pin_0
#define ADC_Channel   ADC_Channel_8
/*********************************************/

//#define ADC1_DR_Address    ((u32)0x40012400+0x4c)

#define ADC3_DR_Address    ((u32)0x40013C4C)
#define ADC1_DR_Address    ((uint32_t)0x4001244C)

__IO uint16_t ADC_ConvertedValue[2];


/*
 * ???:ADC1_GPIO_Config
 * ??  :??ADC1?DMA1???,???PC.01
 * ??  : ?
 * ??  :?
 * ??  :????
 */
static void ADC1_GPIO_Config(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	/* Enable DMA clock */
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	
	/* Enable ADC1 and GPIOC clock */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_GPIO_ADC, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_ADC ;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(GPIO_ADC, &GPIO_InitStructure);
}


/* ???:ADC1_Mode_Config
 * ??  :??ADC1??????MDA??
 * ??  : ?
 * ??  :?
 * ??  :????
 */
static void ADC1_Mode_Config(void)
{
	DMA_InitTypeDef DMA_InitStructure;
	ADC_InitTypeDef ADC_InitStructure;
	
	/* DMA channel1 configuration */
	DMA_DeInit(DMA1_Channel1);
	DMA_InitStructure.DMA_PeripheralBaseAddr = ADC1_DR_Address;	 //ADC??
	DMA_InitStructure.DMA_MemoryBaseAddr = (u32)&ADC_ConvertedValue;//????
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_BufferSize = 2;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;//??????
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;  //??????
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;	//??
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;		//????
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
	DMA_Init(DMA1_Channel1, &DMA_InitStructure);
	
	/* Enable DMA channel1 */
	DMA_Cmd(DMA1_Channel1, ENABLE);
	
	/* ADC1 configuration */
	
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;	//??ADC??
	ADC_InitStructure.ADC_ScanConvMode = ENABLE ; 	 //??????,???????????
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;	//????????,??????ADC??
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;	//?????????
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right; 	//???????
	ADC_InitStructure.ADC_NbrOfChannel = 1;	 	//????????
	ADC_Init(ADC1, &ADC_InitStructure);
	
	/*??ADC??,?PCLK2?6??,?12MHz,ADC????????14MHz*/
	RCC_ADCCLKConfig(RCC_PCLK2_Div6); 
	/*??ADC1???11?55.	5?????,???1 */ 
	ADC_RegularChannelConfig(ADC1, ADC_Channel, 1, ADC_SampleTime_55Cycles5);


	/* Enable ADC1 DMA */
	ADC_DMACmd(ADC1, ENABLE);
	
	/* Enable ADC1 */
	ADC_Cmd(ADC1, ENABLE);
	
	/*??????? */   
	ADC_ResetCalibration(ADC1);
	/*??????????? */
	while(ADC_GetResetCalibrationStatus(ADC1));
	
	/* ADC?? */
	ADC_StartCalibration(ADC1);
	/* ??????*/
	while(ADC_GetCalibrationStatus(ADC1));
	
	/* ??????????,????????ADC?? */ 
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

/*
 * ???:ADC1_Init
 * ??  :?
 * ??  :?
 * ??  :?
 * ??  :????
 */
void ADC1_Init(void)
{
	ADC1_GPIO_Config();
	ADC1_Mode_Config();
}

s16 voltage = 4000;//单位 1mv
#define power0 3700
#define power1 3750

void Voltage_Check()//20HZ
{
	static u16 cnt0,cnt1;
	
	voltage += 0.2f *(2 *(3300 *ADC_ConvertedValue[0]/4096) - voltage);
	
	if(ALL_flag.unlock)//飞行过程中不判断低压
	{
		return;
	}
	else//不飞行的时候的低压判断
	{
		if(voltage < power0 && voltage >3400)//低压
		{
			cnt0++;
			cnt1=0;
			if(cnt0>100)
			{
				cnt0 = 100;
//				if(LED_warn==0)
//				{
//					flag.low_power=1;
//					LED_warn = 1;
//				}
			}
		}
		else if(voltage > power1)//正常
		{
			cnt1++;
			cnt0=0;
			if(cnt1>100)
			{
				cnt1 = 100;
//				if(LED_warn==1)
//				{
//					flag.low_power=0;
//					LED_warn = 0;
//				}
			}
		}
		else
		{
			cnt0=0;
			cnt1=0;
		}
	}
}


//uint16_t ADC_Value[10];
//  /* Configure PC.0-5 (ADC Channel10-15) as analog input -------------------------*/

//void ADC_DMA_Init(void)
//{
//  ADC_InitTypeDef ADC_InitStructure;
//  DMA_InitTypeDef DMA_InitStructure;
//  GPIO_InitTypeDef GPIO_InitStructure;

//  /* Enable DMA1 clock */
//  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

//  /* Enable ADC1 and GPIOC clock */
//  RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOB, ENABLE);
//  
//  /* Configure PC.0-5 (ADC Channel10-15) as analog input -------------------------*/
//  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 ;
//  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
//  GPIO_Init(GPIOB, &GPIO_InitStructure);
//    /* DMA1 channel1 configuration ----------------------------------------------*/
//  DMA_DeInit(DMA1_Channel1);
//  DMA_InitStructure.DMA_PeripheralBaseAddr = ADC1_DR_Address;
//  DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)ADC_Value;
//  DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
//  DMA_InitStructure.DMA_BufferSize = 10;
//  DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
//  DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
//  DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
//  DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
//  DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
//  DMA_InitStructure.DMA_Priority = DMA_Priority_High;
//  DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
//  DMA_Init(DMA1_Channel1, &DMA_InitStructure);
//  
//  /* Enable DMA1 channel1 */
//  DMA_Cmd(DMA1_Channel1, ENABLE);
//  
//  /* ADC1 configuration ------------------------------------------------------*/
//  ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
//  ADC_InitStructure.ADC_ScanConvMode = ENABLE;
//  ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
//  ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
//  ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
//  ADC_InitStructure.ADC_NbrOfChannel = 1;
//  ADC_Init(ADC1, &ADC_InitStructure);

//  /* ADC1 regular channel configuration */ 
//  ADC_RegularChannelConfig(ADC1, ADC_Channel_8, 1, ADC_SampleTime_55Cycles5);

//  /* Enable ADC1 DMA */
//  ADC_DMACmd(ADC1, ENABLE);
//  
//  /* Enable ADC1 */
//  ADC_Cmd(ADC1, ENABLE);

//  /* Enable ADC1 reset calibaration register */   
//  ADC_ResetCalibration(ADC1);
//  /* Check the end of ADC1 reset calibration register */
//  while(ADC_GetResetCalibrationStatus(ADC1));

//  /* Start ADC1 calibaration */
//  ADC_StartCalibration(ADC1);
//  /* Check the end of ADC1 calibration */
//  while(ADC_GetCalibrationStatus(ADC1));
//     
//  /* Start ADC1 Software Conversion */ 
//  ADC_SoftwareStartConvCmd(ADC1, ENABLE);
//}

//uint16_t GetADCValue(void)
//{
// uint8_t i;
// uint32_t tmp;
// for(i=0;i<10;i++)
//	tmp +=ADC_Value[i];
// return (tmp/10);
//}
