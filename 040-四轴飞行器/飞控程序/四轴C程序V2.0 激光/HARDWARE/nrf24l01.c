
#include "ALL_DATA.h"
#include "nrf24l01.h"
#include "SPI.h"
#include <string.h>
#include "LED.h"
#include "ANO_DT.h"
#include "sys.h"

#undef SUCCESS
#define SUCCESS 0
#undef FAILED
#define FAILED  1


#define MAX_TX  		0x10  //达到最大发送次数中断
#define TX_OK   		0x20  //TX发送完成中断
#define RX_OK   		0x40  //接收到数据中断

//**************************************************************************************
//*********************************************NRF24L01*************************************
#define RX_DR				6		//
#define TX_DS				5
#define MAX_RT			4


u8 MPU_Err=1,NRF_Err=1,SPL_Err=1;
uint8_t NRF_SSI,NRF_SSI_CNT;//NRF信号强度
uint16_t Nrf_Erro;


u8 _CH;   //信道
uint8_t NRF24L01_2_RXDATA[RX_PLOAD_WIDTH];//nrf24l01接收到的数据
uint8_t NRF24L01_2_TXDATA[RX_PLOAD_WIDTH];//nrf24l01需要发送的数据

//配对密码
const uint8_t TX_ADDRESS[]= {0xAA,0xBB,0xCC,0x00,0x01};	//本地地址
const uint8_t RX_ADDRESS[]= {0xAA,0xBB,0xCC,0x00,0x01};	//接收地址 

//////////////////////////////////////////////////////////////////////////////////////////////////////////
	


	//24L01操作线   
	#define Set_NRF24L01_CSN    (NRF_CSN_GP->BSRR = NRF24L01_CSN)  // 
	#define Clr_NRF24L01_CSN    (NRF_CSN_GP->BRR = NRF24L01_CSN)   //  
	#define Set_NRF24L01_CE 		(NRF_CE_GP->BSRR = NRF24L01_CE)    // 
	#define Clr_NRF24L01_CE  		(NRF_CE_GP->BRR = NRF24L01_CE)     //  
	#define READ_NRF24L01_IRQ   (NRF_IRQ_GP->IDR&NRF24L01_IRQ)     //IRQ主机数据输入 

//初始化24L01的IO口
void NRF24L01_Configuration(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);    //使能GPIO的时钟  CE
	
	GPIO_InitStructure.GPIO_Pin = NRF24L01_CE;          //NRF24L01  
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;    //推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(NRF_CE_GP, &GPIO_InitStructure);

//	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);   //使能GPIO的时钟 CSN
	GPIO_InitStructure.GPIO_Pin = NRF24L01_CSN;      
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;    //推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(NRF_CSN_GP, &GPIO_InitStructure); 
	
	Set_NRF24L01_CE;                                    //初始化时先拉高
	Set_NRF24L01_CSN;                                   //初始化时先拉高

    //配置NRF2401的IRQ
	GPIO_InitStructure.GPIO_Pin = NRF24L01_IRQ;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU  ;     //上拉输入
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(NRF_IRQ_GP, &GPIO_InitStructure);
	
//	GPIO_SetBits(GPIOA,GPIO_Pin_8);

	SPI_Config();                               			 //初始化SPI
	Clr_NRF24L01_CE; 	                                	//使能24L01
	Set_NRF24L01_CSN;                                   //SPI片选取消
}
//上电检测NRF24L01是否在位
//写5个数据然后再读回来进行比较，
//相同时返回值:0，表示在位;否则返回1，表示不在位	
u8 NRF24L01_Check(void)
{
	u8 buf[5]={0XA5,0XA5,0XA5,0XA5,0XA5};
	u8 buf1[5];
	u8 i;   	 
	NRF24L01_Write_Buf(SPI_WRITE_REG+TX_ADDR,buf,5);//写入5个字节的地址.	
	NRF24L01_Read_Buf(TX_ADDR,buf1,5);              //读出写入的地址  	
	for(i=0;i<5;i++)if(buf1[i]!=0XA5)break;					   
	if(i!=5)return 1;                               //NRF24L01不在位	
	return 0;		                                    //NRF24L01在位
}	 	 
//通过SPI写寄存器
u8 NRF24L01_Write_Reg(u8 regaddr,u8 data)
{
	u8 status;	
    Clr_NRF24L01_CSN;                    //使能SPI传输
  	status =SPI_RW(regaddr); //发送寄存器号 
  	SPI_RW(data);            //写入寄存器的值
  	Set_NRF24L01_CSN;                    //禁止SPI传输	   
  	return(status);       		         //返回状态值
}
//读取SPI寄存器值 ，regaddr:要读的寄存器
u8 NRF24L01_Read_Reg(u8 regaddr)
{
	u8 reg_val;	    
	Clr_NRF24L01_CSN;                //使能SPI传输		
  	SPI_RW(regaddr);     //发送寄存器号
  	reg_val=SPI_RW(0XFF);//读取寄存器内容
  	Set_NRF24L01_CSN;                //禁止SPI传输		    
  	return(reg_val);                 //返回状态值
}	
//在指定位置读出指定长度的数据
//*pBuf:数据指针
//返回值,此次读到的状态寄存器值 
u8 NRF24L01_Read_Buf(u8 regaddr,u8 *pBuf,u8 datalen)
{
	u8 status,u8_ctr;	       
  	Clr_NRF24L01_CSN;                     //使能SPI传输
  	status=SPI_RW(regaddr);   //发送寄存器值(位置),并读取状态值   	   
	for(u8_ctr=0;u8_ctr<datalen;u8_ctr++)pBuf[u8_ctr]=SPI_RW(0XFF);//读出数据
  	Set_NRF24L01_CSN;                     //关闭SPI传输
  	return status;                        //返回读到的状态值
}
//在指定位置写指定长度的数据
//*pBuf:数据指针
//返回值,此次读到的状态寄存器值
u8 NRF24L01_Write_Buf(u8 regaddr, u8 *pBuf, u8 datalen)
{
	u8 status,u8_ctr;	    
	Clr_NRF24L01_CSN;                                    //使能SPI传输
  	status = SPI_RW(regaddr);                //发送寄存器值(位置),并读取状态值
  	for(u8_ctr=0; u8_ctr<datalen; u8_ctr++)SPI_RW(*pBuf++); //写入数据	 
  	Set_NRF24L01_CSN;                                    //关闭SPI传输
  	return status;                                       //返回读到的状态值
}				   
//启动NRF24L01发送一次数据
//txbuf:待发送数据首地址
//返回值:发送完成状况
u8 NRF24L01_TxPacket(u8 *txbuf)
{
	u8 state;   
	Clr_NRF24L01_CE;
	NRF24L01_Write_Buf(WR_TX_PLOAD,txbuf,TX_PLOAD_WIDTH);//写数据到TX BUF  32个字节
	Set_NRF24L01_CE;                                     //启动发送	   
	while(READ_NRF24L01_IRQ!=0);                         //等待发送完成
	state=NRF24L01_Read_Reg(STATUS);                     //读取状态寄存器的值	   
	NRF24L01_Write_Reg(SPI_WRITE_REG+STATUS,state);      //清除TX_DS或MAX_RT中断标志
	if(state&MAX_TX)                                     //达到最大重发次数
	{
		NRF24L01_Write_Reg(FLUSH_TX,0xff);               //清除TX FIFO寄存器 
		return MAX_TX; 
	}
	if(state&TX_OK)                                      //发送完成
	{
		return SUCCESS;
	}
	return FAILED;                                         //其他原因发送失败
}

//启动NRF24L01发送一次数据
//txbuf:待发送数据首地址
//返回值:0，接收完成；其他，错误代码
u8 NRF24L01_RxPacket(u8 *rxbuf)
{
	u8 state;		    							      
	state=NRF24L01_Read_Reg(STATUS);                //读取状态寄存器的值    	 
	NRF24L01_Write_Reg(SPI_WRITE_REG+STATUS,state); //清除TX_DS或MAX_RT中断标志
	if(state&RX_OK)                                 //接收到数据
	{
		NRF24L01_Read_Buf(RD_RX_PLOAD,rxbuf,RX_PLOAD_WIDTH);//读取数据
		NRF24L01_Write_Reg(FLUSH_RX,0xff);          //清除RX FIFO寄存器 
		return SUCCESS; 
	}	   
	return FAILED;                                      //没收到任何数据
}

//该函数初始化NRF24L01到RX模式
//设置RX地址,写RX数据宽度,选择RF频道,波特率和LNA HCURR
//当CE变高后,即进入RX模式,并可以接收数据了		   
void RX_Mode(void)
{
	Clr_NRF24L01_CE;	  
    //写RX节点地址
  	NRF24L01_Write_Buf(SPI_WRITE_REG+RX_ADDR_P0,(u8*)RX_ADDRESS,RX_ADR_WIDTH);

    //使能通道0的自动应答    
  	NRF24L01_Write_Reg(SPI_WRITE_REG+EN_AA,0x01);    
    //使能通道0的接收地址  	 
  	NRF24L01_Write_Reg(SPI_WRITE_REG+EN_RXADDR,0x01); 
	  //设置RF通信频率		  
  	NRF24L01_Write_Reg(SPI_WRITE_REG+RF_CH,45);	  //(要改单机控制，就把45改成跟遥控器单独一样的。就可以单机控制了)
    //选择通道0的有效数据宽度 	    
  	NRF24L01_Write_Reg(SPI_WRITE_REG+RX_PW_P0,RX_PLOAD_WIDTH);
    //设置TX发射参数,0db增益,2Mbps,低噪声增益开启   
  	NRF24L01_Write_Reg(SPI_WRITE_REG+RF_SETUP,0x0f);
    //配置基本工作模式的参数;PWR_UP,EN_CRC,16BIT_CRC,PRIM_RX接收模式 
  	NRF24L01_Write_Reg(SPI_WRITE_REG+NCONFIG, 0x0f); 
    //CE为高,进入接收模式 
  	Set_NRF24L01_CE;                                
}			

//该函数初始化NRF24L01到TX模式
//设置TX地址,写TX数据宽度,设置RX自动应答的地址,填充TX发送数据,
//选择RF频道,波特率和LNA HCURR PWR_UP,CRC使能
//当CE变高后,即进入RX模式,并可以接收数据了		   
//CE为高大于10us,则启动发送.	 
void TX_Mode(void)
{														 
	Clr_NRF24L01_CE;	    
    //写TX节点地址 
  	NRF24L01_Write_Buf(SPI_WRITE_REG+TX_ADDR,(u8*)TX_ADDRESS,TX_ADR_WIDTH);    
    //设置TX节点地址,主要为了使能ACK	  
  	NRF24L01_Write_Buf(SPI_WRITE_REG+RX_ADDR_P0,(u8*)RX_ADDRESS,RX_ADR_WIDTH); 

    //使能通道0的自动应答    
  	NRF24L01_Write_Reg(SPI_WRITE_REG+EN_AA,0x01);     
    //使能通道0的接收地址  
  	NRF24L01_Write_Reg(SPI_WRITE_REG+EN_RXADDR,0x01); 
    //设置自动重发间隔时间:500us + 86us;最大自动重发次数:10次
  	NRF24L01_Write_Reg(SPI_WRITE_REG+SETUP_RETR,0x1a);
    //设置RF通道为40
  	NRF24L01_Write_Reg(SPI_WRITE_REG+RF_CH,45);       //(要改单机控制，就把45改成跟遥控器单独一样的。就可以单机控制了)
    //设置TX发射参数,0db增益,2Mbps,低噪声增益开启   
  	NRF24L01_Write_Reg(SPI_WRITE_REG+RF_SETUP,0x0f);  //0x27  250K   0x07 1M
    //配置基本工作模式的参数;PWR_UP,EN_CRC,16BIT_CRC,PRIM_RX发送模式,开启所有中断
  	NRF24L01_Write_Reg(SPI_WRITE_REG+NCONFIG,0x0e);    
    // CE为高,10us后启动发送
	Set_NRF24L01_CE;                                  
}	

void ANO_NRF_TxPacket_AP(uint8_t * tx_buf, uint8_t len)
{	
	Clr_NRF24L01_CE;		 													//StandBy I模式
	NRF24L01_Write_Buf(0xa8, tx_buf, len); 			 //装载数据
	Set_NRF24L01_CE;  
}

void ANO_NRF_Init(u8 model, u8 ch)
{
	Clr_NRF24L01_CE;
	
	NRF24L01_Write_Buf(SPI_WRITE_REG+RX_ADDR_P0,(u8*)RX_ADDRESS,RX_ADR_WIDTH);	//写RX节点地址
	NRF24L01_Write_Buf(SPI_WRITE_REG+TX_ADDR,(u8*)TX_ADDRESS,TX_ADR_WIDTH); 		//写TX节点地址 
	NRF24L01_Write_Reg(SPI_WRITE_REG+EN_AA,0x01); 															//使能通道0的自动应答
	NRF24L01_Write_Reg(SPI_WRITE_REG+EN_RXADDR,0x01);														//使能通道0的接收地址
	NRF24L01_Write_Reg(SPI_WRITE_REG+SETUP_RETR,0x1a);													//设置自动重发间隔时间:500us;最大自动重发次数:10次 2M波特率下
	NRF24L01_Write_Reg(SPI_WRITE_REG+RF_CH,ch);																	//设置RF通道为CHANAL
	NRF24L01_Write_Reg(SPI_WRITE_REG+RF_SETUP,0x0f); 														//设置TX发射参数,0db增益,2Mbps,低噪声增益开启
	//Write_Reg(NRF_WRITE_REG+RF_SETUP,0x07); 												    			//设置TX发射参数,0db增益,1Mbps,低噪声增益开启
	//Write_Reg(NRF_WRITE_REG+RF_SETUP,0x27); 												   				//设置TX发射参数,0db增益,250Kbps,低噪声增益开启
/////////////////////////////////////////////////////////
	if(model==1)				//RX
	{
		NRF24L01_Write_Reg(SPI_WRITE_REG+RX_PW_P0,RX_PLOAD_WIDTH);								//选择通道0的有效数据宽度 
		NRF24L01_Write_Reg(SPI_WRITE_REG + NCONFIG, 0x0f);   		        					// IRQ收发完成中断开启,16位CRC,主接收
	}
	else if(model==2)		//TX
	{
		NRF24L01_Write_Reg(SPI_WRITE_REG+RX_PW_P0,RX_PLOAD_WIDTH);								//选择通道0的有效数据宽度 
		NRF24L01_Write_Reg(SPI_WRITE_REG + NCONFIG, 0x0e);   		 									// IRQ收发完成中断开启,16位CRC,主发送
	}
	else if(model==3)		//RX2
	{
		NRF24L01_Write_Reg(FLUSH_TX,0xff);
		NRF24L01_Write_Reg(FLUSH_RX,0xff);
		NRF24L01_Write_Reg(SPI_WRITE_REG + NCONFIG, 0x0f);   		  								// IRQ收发完成中断开启,16位CRC,主接收
		
		SPI_RW(0x50);
		SPI_RW(0x73);
		NRF24L01_Write_Reg(SPI_WRITE_REG+0x1c,0x01);
		NRF24L01_Write_Reg(SPI_WRITE_REG+0x1d,0x06);
	}
	else								//TX2
	{
		NRF24L01_Write_Reg(SPI_WRITE_REG + NCONFIG, 0x0e);   											// IRQ收发完成中断开启,16位CRC,主发送
		NRF24L01_Write_Reg(FLUSH_TX,0xff);
		NRF24L01_Write_Reg(FLUSH_RX,0xff);
		
		SPI_RW(0x50);
		SPI_RW(0x73);
		NRF24L01_Write_Reg(SPI_WRITE_REG+0x1c,0x01);
		NRF24L01_Write_Reg(SPI_WRITE_REG+0x1d,0x06);
	}
		Set_NRF24L01_CE;  
}
void NRF24L01_init(void)
{
	NRF24L01_Configuration();
	
	bLED_H();	 	//前灯灭
	aLED_H();		//前灯灭
	fLED_L();		//后灯亮
	hLED_L();		//后灯亮
  do
	{ 
		GetLockCode();
		_CH = ST_CpuID % 0x7E;      //芯片ID用来做通讯对频通道
		ANO_NRF_Init(MODEL_RX2,_CH);
		NRF_Err=1;
	}while(NRF24L01_Check() == FAILED);
	NRF_Err=0;
}
//////////////////////////////////////////////////////////////


void ANO_NRF_Check_Event(void)
{
	u8 sta = NRF24L01_Read_Reg(SPI_READ_REG + STATUS);   //读接收标志
	//printf("sta: %d   \r\n",sta);  //串口1输出
	if(sta & (1<<RX_DR))//判断是否收到数据
	{
		u8 rx_len = NRF24L01_Read_Reg(R_RX_PL_WID);       
		if(rx_len<33)
		{
			NRF24L01_Read_Buf(RD_RX_PLOAD,NRF24L01_2_RXDATA,rx_len); //接收数据
			Nrf_Erro = 0;
		}
		else 
		{
			NRF24L01_Write_Reg(FLUSH_RX,0xff);//清除缓冲区
		}
	}
	if(sta & (1<<TX_DS))//
	{
		
	}
	if(sta & (1<<MAX_RT))//判断是否最大重试次数
	{
		if(sta & 0x01)	//TX FIFO FULL
		{
			NRF24L01_Write_Reg(FLUSH_TX,0xff);
		}
	}
	////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////
	NRF24L01_Write_Reg(SPI_WRITE_REG + STATUS, sta);//清除接收标志

}

u8 NRF_Connect(void)//1KHZ
{
	static u8 Connect_flag;
	
	Nrf_Erro ++;
	if(Nrf_Erro==1)
	{
		ANO_DT_Data_Receive_Anl(NRF24L01_2_RXDATA,NRF24L01_2_RXDATA[3]+5);//解析2.4G数据
		NRF_SSI_CNT++;
	//	printf("NRF_SSI_CNT: %d   \r\n",NRF_SSI_CNT);  //串口1输出

		Connect_flag = 1;
	}
	if(Nrf_Erro>=500)
	{
		Nrf_Erro = 1;
		Connect_flag = 0;
	}
	return Connect_flag;
}



/*********************END OF FILE******************************************************/





















/*********************END OF FILE******************************************************/
















