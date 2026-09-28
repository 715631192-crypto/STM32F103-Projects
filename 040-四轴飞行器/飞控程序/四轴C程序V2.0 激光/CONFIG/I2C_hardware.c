#include "stm32f10x.h"
#include "system_stm32f10x.h"
#include "I2C.h"

#define I2C1_OWN_ADDRESS7 0xcc
#undef SUCCESS
#define SUCCESS 0
#undef FAILED
#define FAILED  1

/**
  *****************************************************************************
  * @Name   : 硬件IIC初始化
  *
  * @Brief  : none
  *
  * @Input  : I2Cx:           IIC组
  *           SlaveAdd:       作为从设备时识别地址
  *           F103IIC1_Remap: 针对F103的IIC1是否重映射
  *                           0: 不重映射
  *                           1: 重映射。PB.06 -> PB.08；PB.07 -> PB.09
  *
  * @Output : none
  *
  * @Return : none
  *****************************************************************************
**/
void Hard_IIC_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	I2C_InitTypeDef I2C_InitStructure;
	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
	
	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	//
	//管脚复用
	//
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_I2C1);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_I2C1);
		
	//
	//配置IIC参数
	//
	I2C_InitStructure.I2C_Mode                = I2C_Mode_I2C;
	I2C_InitStructure.I2C_DutyCycle           = I2C_DutyCycle_2;
	I2C_InitStructure.I2C_OwnAddress1         = 0xcc;  //从设备地址
	I2C_InitStructure.I2C_Ack                 = I2C_Ack_Enable;
	I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_InitStructure.I2C_ClockSpeed          = (400 * 1000);  //SCL最大100KHz
	
	
	I2C_Cmd(I2C1, ENABLE);
	I2C_Init(I2C1, &I2C_InitStructure);
	I2C_AcknowledgeConfig(I2C1, ENABLE); 
	
	//
	//禁止时钟延长
	//
	I2C1->CR1 |= 1<<7;
}

/**
  *****************************************************************************
  * @Name   : 硬件IIC等待从设备内部操作完成
  *
  * @Brief  : none
  *
  * @Input  : I2Cx:     IIC组
  *           SlaveAdd: 作为从设备时识别地址
  *           ReadAdd:  读取的EEPROM内存地址
  *
  * @Output : *err:     返回的错误值
  *
  * @Return : 读取到的数据
  *****************************************************************************
**/
void Hard_IICWaiteStandby(uint8_t SlaveAdd)
{
	u16 tmp = 0;
	
	I2C1->SR1 &= 0x0000;  //清除状态寄存器1
	
	do
	{
		I2C_GenerateSTART(I2C1, ENABLE);  //产生起始信号
		tmp = I2C1->SR1;  //读取SR1寄存器，然后写入数据寄存器操作来清除SB位，EV5
		I2C_Send7bitAddress(I2C1, SlaveAdd, I2C_Direction_Transmitter);  //发送从设备地址
	} while ((I2C1->SR1 & 0x0002) == 0x0000);  //当ADDR = 1时，表明应答了，跳出循环
	I2C_ClearFlag(I2C1, I2C_FLAG_AF);  //清除应答失败标志位
	I2C_GenerateSTOP(I2C1, ENABLE);  //发送停止信号
}

/**
  *****************************************************************************
  * @Name   : 硬件IIC发送一个字节数据
  *
  * @Brief  : none
  *
  * @Input  : I2Cx:     IIC组
  *           SlaveAdd: 作为从设备时识别地址
  *           WriteAdd: 写入EEPROM内存地址
  *           Data:     写入的数据
  *
  * @Output : *err:     返回的错误值
  *
  * @Return : none
  *****************************************************************************
**/
u8 Hard_IICWriteOneByte(uint8_t SlaveAdd, u8 WriteAdd, u8 Data)
{
	u16 temp = 0;
	u16 flag = 0;
	
	while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY))  //等待IIC
	{
		temp++;
		if (temp > 800)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
			return FAILED;
		}
	}
	
	I2C_GenerateSTART(I2C1, ENABLE);  //产生起始信号
	temp = 0;
	//
	//EV5
	//
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT))
	{
		temp++;
		if (temp > 800)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
			return FAILED;
		}
	}
	
	I2C_Send7bitAddress(I2C1,SlaveAdd, I2C_Direction_Transmitter);  //发送设备地址
	temp = 0;
	//
	//EV6
	//
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
	{
		temp++;
		if (temp > 1000)
		{
			I2C1->CR1 |= I2C_CR1_STOP;  //产生停止信号
			return FAILED;
		}
	}
	//
	//读取SR2状态寄存器
	//
	flag = I2C1->SR2;  //软件读取SR1寄存器后,对SR2寄存器的读操作将清除ADDR位，不可少！！！！！！！！！
	
	I2C1->DR = WriteAdd;  //发送存储地址
	temp = 0;
	//
	//EV8
	//
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTING))
	{
		temp++;
		if (temp > 800)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
			return FAILED;
		}
	}
	I2C_SendData(I2C1, Data);  //发送数据
	temp = 0;
	//
	//EV8_2
	//
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
	{
		temp++;
		if (temp > 800)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
			return FAILED;
		}
	}
	I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
	return SUCCESS;
}

/**
  *****************************************************************************
  * @Name   : 硬件IIC读取一个字节数据
  *
  * @Brief  : none
  *
  * @Input  : I2Cx:     IIC组
  *           SlaveAdd: 作为从设备时识别地址
  *           ReadAdd:  读取的EEPROM内存地址
  *
  * @Output : *err:     返回的错误值
  *
  * @Return : 读取到的数据
  *****************************************************************************
**/
u8 Hard_IIC_ReadOneByte(uint8_t SlaveAdd, u8 ReadAdd,u8 *REG_data)
{
	u16 i = 0;
	u8 temp = 0;
	u16 flag = 0;
	
	while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY))  //等待IIC
	{
		i++;
		if (i > 800)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
			return FAILED;
		}
	}
	I2C_GenerateSTART(I2C1, ENABLE);  //发送起始信号
	i = 0;
	//
	//EV5
	//
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT))
	{
		i++;
		if (i > 800)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
			return FAILED;
		}
	}
	I2C_Send7bitAddress(I2C1, SlaveAdd, I2C_Direction_Transmitter);  //发送设备地址
	i = 0;
	//
	//EV6
	//
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
	{
		i++;
		if (i > 800)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
			return FAILED;
		}
	}
	flag = I2C1->SR2;  //软件读取SR1寄存器后,对SR2寄存器的读操作将清除ADDR位，不可少！！！！！！！！！
	
	I2C_SendData(I2C1, ReadAdd);  //发送存储地址
	i = 0;
	//
	//EV8
	//
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTING))
	{
		i++;
		if (i > 2000)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
			return FAILED;
		}
	}
	I2C_GenerateSTOP(I2C1, ENABLE);
	I2C_GenerateSTART(I2C1, ENABLE);  //重启信号
	i = 0;
	//
	//EV5
	//
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT))
	{
		i++;
		if (i > 800)
		{			
			I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
			return FAILED;
		}
	}
	I2C_Send7bitAddress(I2C1, SlaveAdd, I2C_Direction_Receiver);  //读取命令
	i = 0;
	//
	//EV6
	//
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED))
	{
		i++;
		if (i > 800)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
			return FAILED;
		}
	}
	flag = I2C1->SR2;
	
	I2C_AcknowledgeConfig(I2C1, DISABLE);  //发送NACK
	//I2C_GenerateSTOP(I2C1, ENABLE);
	i = 0;
	//
	//EV7
	//
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_RECEIVED))
	{
		i++;
		if (i > 800)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
			return FAILED;
		}
	}
	*REG_data = I2C_ReceiveData(I2C1);
	I2C_AcknowledgeConfig(I2C1, ENABLE);
	I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
	return SUCCESS;
}

/**
  *****************************************************************************
  * @Name   : 硬件IIC发送多个字节数据
  *
  * @Brief  : none
  *
  * @Input  : I2Cx:       IIC组
  *           SlaveAdd:   作为从设备时识别地址
  *           WriteAdd:   写入EEPROM内存起始地址
  *           NumToWrite: 写入数据量
  *           *pBuffer:   写入的数据组缓存
  *
  * @Output : *err:     返回的错误值
  *
  * @Return : none
  *****************************************************************************
**/
u8 Hard_IIC_WriteNByte(u8 SlaveAdd, u8 WriteAdd, u8 NumToWrite, u8 * pBuffer)
{
	u16 sta = 0;
	u16 temp = 0;
	
	while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY))  //等待IIC
	{
		temp++;
		if (temp > 800)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
			return FAILED;
		}
	}
	I2C_GenerateSTART(I2C1, ENABLE);  //产生起始信号
	temp = 0;
	//
	//EV5
	//
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT))
	{
		temp++;
		if (temp > 800)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
			return FAILED;
		}
	}
	I2C_Send7bitAddress(I2C1, SlaveAdd, I2C_Direction_Transmitter);  //发送设备地址
	temp = 0;
	//
	//EV6
	//
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
	{
		temp++;
		if (temp > 1000)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
			return FAILED;
		}
	}
	//
	//读取SR2状态寄存器
	//
	sta = I2C1->SR2;  //软件读取SR1寄存器后,对SR2寄存器的读操作将清除ADDR位，不可少！！！！！！！！！
	I2C_SendData(I2C1, WriteAdd);  //发送存储地址
	temp = 0;
	//
	//EV8
	//
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTING))
	{
		temp++;
		if (temp > 800)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
			return FAILED ;
		}
	}
	//
	//循环发送数据
	//
	while (NumToWrite--)
	{
		I2C_SendData(I2C1, *pBuffer);  //发送数据
		pBuffer++;
		temp = 0;
		//
		//EV8_2
		//
		while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
		{
			temp++;
			if (temp > 800)
			{
				I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
				return FAILED;
			}
		}
	}
	I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
	
	return SUCCESS;
}

/**
  *****************************************************************************
  * @Name   : 硬件IIC读取多个字节数据
  *
  * @Brief  : none
  *
  * @Input  : I2Cx:      IIC组
  *           SlaveAdd:  作为从设备时识别地址
  *           ReadAdd:   读取的EEPROM内存起始地址
  *           NumToRead: 读取数量
  *
  * @Output : *pBuffer: 数据输出缓冲区
  *           *err:     返回的错误值
  *
  * @Return : 读取到的数据
  *****************************************************************************
**/
u8 Hard_IIC_ReadNByte(u8 SlaveAdd, u8 ReadAdd,u8 NumToWrite,u8 * pBuffer)
{
	u16 i = 0;
	u8 temp = 0;
	u16 flag = 0;
	
	while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY))  //等待IIC
	{
		i++;
		if (i > 800)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
			return FAILED;
		}
	}
	I2C_GenerateSTART(I2C1, ENABLE);  //发送起始信号
	i = 0;
	//
	//EV5
	//
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT))
	{
		i++;
		if (i > 800)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
			return FAILED;
		}
	}
	I2C_Send7bitAddress(I2C1, SlaveAdd, I2C_Direction_Transmitter);  //发送设备地址
	i = 0;
	//
	//EV6
	//
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
	{
		i++;
		if (i > 800)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
			return FAILED;
		}
	}
	flag = I2C1->SR2;  //软件读取SR1寄存器后,对SR2寄存器的读操作将清除ADDR位，不可少！！！！！！！！！
	
	I2C_SendData(I2C1, ReadAdd);  //发送存储地址
	i = 0;
	//
	//EV8
	//
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTING))
	{
		i++;
		if (i > 2000)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
			return FAILED;
		}
	}
	I2C_GenerateSTOP(I2C1, ENABLE);
	I2C_GenerateSTART(I2C1, ENABLE);  //重启信号
	i = 0;
	//
	//EV5
	//
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT))
	{
		i++;
		if (i > 800)
		{			
			I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
			return FAILED;
		}
	}
	I2C_Send7bitAddress(I2C1, SlaveAdd, I2C_Direction_Receiver);  //读取命令
	i = 0;
	//
	//EV6
	//
	while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED))
	{
		i++;
		if (i > 800)
		{
			I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
			return FAILED;
		}
	}
	flag = I2C1->SR2;
	I2C_AcknowledgeConfig(I2C1, ENABLE);  //发送NACK	
	
	while (NumToWrite)
	{
		i = 0;	

		while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_RECEIVED))
		{
			i++;
			if (i > 800)
			{
				I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
				return FAILED;
			}
		}
        *pBuffer = I2C_ReceiveData(I2C1);	
        if (NumToWrite == 1)
            I2C_AcknowledgeConfig(I2C1, DISABLE);  //发送NACK
        else
            I2C_AcknowledgeConfig(I2C1, ENABLE);  //发送NACK		
        pBuffer++;
        NumToWrite--;
    }
		I2C_GenerateSTOP(I2C1, ENABLE);  //产生停止信号
		return SUCCESS;
}



 
 
 
 
 
 
 
 
