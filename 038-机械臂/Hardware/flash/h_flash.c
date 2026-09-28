#include "./flash/h_flash.h"

SPI_InitTypeDef SPI_InitStructure;
GPIO_InitTypeDef GPIO_InitStructure;

/* LED引脚和SPI引脚重合，切换SPI和LED引脚状态 */
void spi_flash_on(u8 x)
{
    // x:0 关闭spi引脚复用  x:1打开
    if (x)
    {
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // 复用推挽输出
        GPIO_Init(GPIOB, &GPIO_InitStructure);
        GPIO_SetBits(GPIOB, GPIO_Pin_13);

        SPI_Cmd(SPI2, ENABLE); // 使能SPI外设
        spi_write_read(0xff);  // 启动传输
    }
    else
    {
        SPI_Cmd(SPI2, DISABLE); // 失能SPI外设
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(GPIOB, &GPIO_InitStructure);
    }
}

/* 初始化SPI FLASH的IO口 */
void spi_flash_init(void)
{
    // 开启时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // 引脚端口初始化
    // PB12 SPI_CS
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_SetBits(GPIOB, GPIO_Pin_12);

    // PB13 SCK、PB15 MOSI 复用推挽
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // 复用推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // PB14 MISO 上拉输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;     // 上拉输入
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; // 50MHz上输入
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_SetBits(GPIOB, GPIO_Pin_13 | GPIO_Pin_15); // 默认电平

    // SPI初始化
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex; // 双线全双工
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;                      // 主机模式
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;                  // 8位数据
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;                        // 时钟稳态高电平
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;                       // 第二个时钟沿捕获
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;                          // 软件NSS
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB; // 高位先行
    SPI_InitStructure.SPI_CRCPolynomial = 7;           // CRC多项式

    SPI_Init(SPI2, &SPI_InitStructure);
    SPI_Cmd(SPI2, ENABLE);
    spi_write_read(0xff);                   // 启动传输
    spi_set_speed(SPI_BaudRatePrescaler_2); // 设置高速
    spi_flash_on(0);                        // 切回LED模式，PB13与SPI_SCK复用
}

/* SPI速度设置函数 */
void spi_set_speed(uint16_t SpeedSet)
{
    SPI_Cmd(SPI2, DISABLE);
    SPI_InitStructure.SPI_BaudRatePrescaler = SpeedSet;
    SPI_Init(SPI2, &SPI_InitStructure);
    SPI_Cmd(SPI2, ENABLE);
}

/* SPI读写数据，TxData发送的数据，返回字节数据 */
u8 spi_write_read(u8 TxData)
{
    u8 retry = 0;
    // 检查TX发送缓存空位
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET)
    {
        retry++;
        if (retry > 200)
        {
            return 0;
        }
    }
    SPI_I2S_SendData(SPI2, TxData);

    retry = 0;
    // 检查RX接收缓存非空
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET)
    {
        retry++;
        if (retry > 200)
        {
            return 0;
        }
    }
    return SPI_I2S_ReceiveData(SPI2);
}

/* 读取芯片ID，返回0XEF16=W25Q64；0XEF13=W25Q80；0XEF14=W25Q16；0XEF15=W25Q32 */
u16 spi_flash_read_id(void)
{
    u16 Temp = 0;
    SPI_FLASH_CS(0);
    spi_write_read(W25X_ManufactDeviceID);
    spi_write_read(0x00);
    spi_write_read(0x00);
    spi_write_read(0x00);
    Temp |= spi_write_read(0xFF) << 8;
    Temp |= spi_write_read(0xFF);
    SPI_FLASH_CS(1);
    return Temp;
}

/* 读取状态寄存器 */
u8 spi_flash_read_SR(void)
{
    u8 byte = 0;
    SPI_FLASH_CS(0);
    spi_write_read(W25X_ReadStatusReg);
    byte = spi_write_read(0xFF);
    SPI_FLASH_CS(1);
    return byte;
}

/* 写状态寄存器 */
void spi_flash_write_SR(u8 byte)
{
    SPI_FLASH_CS(0);
    spi_write_read(W25X_WriteStatusReg);
    spi_write_read(byte);
    SPI_FLASH_CS(1);
}

/* Flash写使能 WEL置1 */
void spi_flash_write_enable(void)
{
    SPI_FLASH_CS(0);
    spi_write_read(W25X_WriteEnable);
    SPI_FLASH_CS(1);
}

/* Flash写禁止 WEL清零 */
void spi_flash_write_disable(void)
{
    SPI_FLASH_CS(0);
    spi_write_read(W25X_WriteDisable);
    SPI_FLASH_CS(1);
}

/* 读取单字节 */
char spi_flash_read_char(u32 readAddr)
{
    static char tmp;
    SPI_FLASH_CS(0);
    spi_write_read(W25X_ReadData);
    spi_write_read((u8)(readAddr >> 16));
    spi_write_read((u8)(readAddr >> 8));
    spi_write_read((u8)readAddr & 0xFF);
    tmp = spi_write_read(0XFF);
    SPI_FLASH_CS(1);
    return tmp;
}

/* 批量读数据 */
void spi_flash_read(u8 *pBuffer, , u16 NumByteToRu32 ReadAddread)
{
    u16 i;
    SPI_FLASH_CS(0);
    spi_write_read(W25X_ReadData);
    spi_write_read((u8)(ReadAddr >> 16));
    spi_write_read((u8)(ReadAddr >> 8));
    spi_write_read((u8)ReadAddr & 0xFF);
    for (i = 0; i < NumByteToRead; i++)
    {
        pBuffer[i] = spi_write_read(0XFF);
    }
    SPI_FLASH_CS(1);
}

/* 单字节写入 */
void spi_flash_write_char(char tmp, u32 WriteAddr)
{
    spi_flash_write_enable();
    SPI_FLASH_CS(0);
    spi_write_read(W25X_PageProgram);
    spi_write_read((u8)(WriteAddr >> 16));
    spi_write_read((u8)(WriteAddr >> 8));
    spi_write_read((u8)WriteAddr);
    spi_write_read(tmp);
    SPI_FLASH_CS(1);
    spi_flash_wait_busy();
    // UsartPrintf(USART_DEBUG,"Byte has been written.\r\n");
}

/* 带扇区校验写入（先读后擦再写） */
u8 W25Q_BUF[4096]; // 扇区缓冲区
void SpiFlashWriteS(u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    u32 secpos;    // 扇区地址
    u16 secoff;    // 扇区内偏移地址
    u16 secremain; // 扇区内剩余字节数
    u16 i;

    secpos = WriteAddr / 4096;
    secoff = WriteAddr % 4096;
    secremain = 4096 - secoff;

    if (NumByteToWrite <= secremain)
    {
        secremain = NumByteToWrite;
    }

    while (1)
    {
        spi_flash_read(W25Q_BUF, secpos * 4096, 4096);
        for (i = 0; i < secremain; i++)
        {
            if (W25Q_BUF[secoff + i] != 0XFF)
            {
                break;
            }
        }
        if (i < secremain)
        {
            spi_flash_erase_sector(secpos); // 擦除扇区
            for (i = 0; i < secremain; i++)
            {
                W25Q_BUF[i + secoff] = pBuffer[i]; // 写入数据
            }
            spi_flash_write_no_check(W25Q_BUF, secpos * 4096, 4096);
        }
        else
        {
            spi_flash_write_no_check(pBuffer, WriteAddr, secremain);
        }

        if (NumByteToWrite == secremain) // 写入完成
        {
            break;
        }
        else // 写入未完成
        {
            secpos++;
            secoff = 0;
            pBuffer += secremain;
            WriteAddr += secremain;
            NumByteToWrite -= secremain;
            if (NumByteToWrite > 4096)
            {
                secremain = 4096;
            }
            else
            {
                secremain = NumByteToWrite;
            }
        }
    }
}

/* 通用写入函数（自动处理扇区擦除） */
void spi_flash_write(u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    u8 SpiFlash_BUF[4096]; // 扇区缓冲区
    u32 sector = 0; // 扇区地址
    u16 sectorremain = 0; // 扇区内剩余字节数
    u16 i;

    sectorremain = 4096 - WriteAddr % 4096;//计算当前扇区剩余字节数
    if (NumByteToWrite <= sectorremain)
    {
        sectorremain = NumByteToWrite;
    }

    while (1)
    {
        sector = WriteAddr >> 12;//计算当前扇区地址
        spi_flash_read(SpiFlash_BUF, sector * 4096, 4096);
        for (i = 0; i < sectorremain; i++)
        {
            if (SpiFlash_BUF[WriteAddr % 4096 + i] != 0XFF)
            {
                break;
            }
        }
        if (i < sectorremain)
        {
            spi_flash_erase_sector(sector);
            for (i = 0; i < sectorremain; i++)
            {
                SpiFlash_BUF[WriteAddr % 4096 + i] = pBuffer[i];
            }
            spi_flash_write_sector(SpiFlash_BUF, sector * 4096, 4096);
        }
        else
        {
            spi_flash_write_sector(pBuffer, WriteAddr, sectorremain);
        }

        if (NumByteToWrite == sectorremain)
        {
            break;
        }
        else
        {
            pBuffer += sectorremain;
            WriteAddr += sectorremain;
            NumByteToWrite -= sectorremain;
            if (NumByteToWrite > 4096)
            {
                sectorremain = 4096;
            }
            else
            {
                sectorremain = NumByteToWrite;
            }
        }
    }
}

/* 单页写入（最多256字节） */
void spi_flash_write_page(u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    u16 pageremain; // 页内剩余字节数
    pageremain = 256 - WriteAddr % 256; // 计算当前页剩余字节数
    if (NumByteToWrite > pageremain)
    {
        NumByteToWrite = pageremain;
        // UsartPrintf(USART_DEBUG,"There are not enough writable bytes left in this page!\r\n");
    }

    spi_flash_write_enable();
    SPI_FLASH_CS(0);
    spi_write_read(W25X_PageProgram);
    spi_write_read((u8)(WriteAddr >> 16));
    spi_write_read((u8)(WriteAddr >> 8));
    spi_write_read((u8)WriteAddr);
    for (u16 i = 0; i < NumByteToWrite; i++)
    {
        spi_write_read(pBuffer[i]);
    }
    SPI_FLASH_CS(1);
    spi_flash_wait_busy();
    // UsartPrintf(USART_DEBUG,"%s, %d bytes have been written.\r\n",__FUNCTION__,NumByteToWrite);
}

/* 扇区写入（最多4096字节） */
void spi_flash_write_sector(u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    u16 sectorremain;
    sectorremain = 4096 - WriteAddr % 4096;
    if (NumByteToWrite > sectorremain)
    {
        NumByteToWrite = sectorremain;
        // UsartPrintf(USART_DEBUG,"There are not enough writable bytes left in this sector!\r\n");
    }
    spi_flash_write_no_check(pBuffer, WriteAddr, NumByteToWrite);
}

/* 无校验批量写入（自动分页，不预先读扇区校验） */
void spi_flash_write_no_check(u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    u16 pageremain;
    pageremain = 256 - WriteAddr % 256;
    if (NumByteToWrite <= pageremain)
    {
        pageremain = NumByteToWrite;
    }

    while (1)
    {
        spi_flash_write_page(pBuffer, WriteAddr, pageremain);
        if (NumByteToWrite == pageremain)
        {
            break;
        }
        else
        {
            pBuffer += pageremain;
            WriteAddr += pageremain;
            NumByteToWrite -= pageremain;
            if (NumByteToWrite > 256)
            {
                pageremain = 256;
            }
            else
            {
                pageremain = NumByteToWrite;
            }
        }
    }
}

/* 擦除单个扇区，Dst_Addr=扇区号，最少等待150ms */
void spi_flash_erase_sector(u32 Dst_Addr)
{
    Dst_Addr <<= 12;
    spi_flash_write_enable();
    spi_flash_wait_busy(); // 等待Flash空闲
    SPI_FLASH_CS(0);
    spi_write_read(W25X_SectorErase);
    spi_write_read((u8)(Dst_Addr >> 16));
    spi_write_read((u8)(Dst_Addr >> 8));
    spi_write_read((u8)Dst_Addr);
    SPI_FLASH_CS(1);
    spi_flash_wait_busy();
    // UsartPrintf(USART_DEBUG,"Sector has been erased.\r\n");
}

/* 整片芯片擦除 */
void spi_flash_erase_chip(void)
{
    spi_flash_write_enable();
    spi_flash_wait_busy();
    SPI_FLASH_CS(0);
    spi_write_read(W25X_ChipErase);
    SPI_FLASH_CS(1);
    spi_flash_wait_busy();
    // UsartPrintf(USART_DEBUG,"Chip has been erased.\r\n");
}

/* 等待Flash空闲（轮询BUSY位） */
void spi_flash_wait_busy(void)
{
    while ((spi_flash_read_SR() & 0x01) == 0x01)
        ;
}

/* Flash进入掉电模式 */
void spi_flash_power_down(void)
{
    SPI_FLASH_CS(0);
    spi_write_read(W25X_PowerDown);
    SPI_FLASH_CS(1);
    Delay_ms(1); // 等待Tpd
}

/* 唤醒Flash */
void spi_flash_wake_up(void)
{
    SPI_FLASH_CS(0);
    spi_write_read(W25X_ReleasePowerDown);
    SPI_FLASH_CS(1);
    Delay_ms(1); // 等待TRES1
}
