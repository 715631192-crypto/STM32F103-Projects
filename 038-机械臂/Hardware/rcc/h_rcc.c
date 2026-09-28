
#include "./rcc/h_rcc.h"

void rcc_Init(void)
{
    ErrorStatus HSEStarUpStatus;
    //rcc reset
    RCC_DeInit();
    //开启外部时钟
    RCC_HSEConfig(RCC_HSE_ON);
    //等待外部时钟准备好
    HSEStarUpStatus=RCC_WaitForHSEStartUp();
    //启动失败 在此等待
    while (HSEStarUpStatus==ERROR);
    RCC_HCLKConfig(RCC_HCLK_Div1);//SYSCLK
    RCC_PCLK1Config(RCC_HCLK_Div2);//PCLK1
    RCC_PCLK2Config(RCC_HCLK_Div1);//PCLK2
    //外部时钟为8M 倍频
    RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
    RCC_PLLCmd(ENABLE);
    while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
    while(RCC_GetSYSCLKSource() != 0x08);
    return;
}
