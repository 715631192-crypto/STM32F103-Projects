#include "w25q64.h"
#include "delay.h"

/* 引脚：PB12=CS（软件控制），SPI2 复用 PB13/PB14/PB15 */
#define W25_CS_H()  GPIO_SetBits(GPIOB, GPIO_Pin_12)
#define W25_CS_L()  GPIO_ResetBits(GPIOB, GPIO_Pin_12)

/* ---------------- 底层 SPI2 收发 ---------------- */

/** @brief SPI2 收发一个字节（全双工，发什么收什么） */
static uint8_t w25_spi_rw(uint8_t b)
{
    SPI_I2S_SendData(SPI2, b);
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET)
        ;
    return (uint8_t)SPI_I2S_ReceiveData(SPI2);
}

/* ---------------- 基础命令 ----------------
 * 指令表（W25Qxx 系列）：
 *   0x06 写使能     0x05 读状态寄存器
 *   0x03 读数据     0x02 页编程(写，一页最多256B)
 *   0x20 4KB 扇区擦除
 */

/** @brief 读状态寄存器1 */
static uint8_t w25_read_sr(void)
{
    uint8_t v;
    W25_CS_L();
    w25_spi_rw(0x05);
    v = w25_spi_rw(0xFF);
    W25_CS_H();
    return v;
}

/** @brief 写使能（每次擦/写之前必须先发） */
static void w25_wren(void)
{
    W25_CS_L();
    w25_spi_rw(0x06);
    W25_CS_H();
}

/** @brief 等待内部擦写完成（状态寄存器 BUSY 位） */
static void w25_wait_busy(void)
{
    while (w25_read_sr() & 0x01)
        ;   /* BUSY=1 表示正在擦写 */
}

/* ---------------- 对外接口 ---------------- */

/**
 * @brief SPI2 + W25Q64 初始化，并探测器件 ID
 * @return 1 = ID 正确（0xEF4017）
 */
uint8_t W25Q64_Init(void)
{
    GPIO_InitTypeDef g;
    SPI_InitTypeDef  s;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    g.GPIO_Pin   = GPIO_Pin_12;
    g.GPIO_Mode  = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &g);
    W25_CS_H();

    g.GPIO_Pin   = GPIO_Pin_13 | GPIO_Pin_15;
    g.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &g);
    g.GPIO_Pin   = GPIO_Pin_14;
    g.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &g);

    /* SPI2 挂 APB1(36MHz)，4 分频 = 9MHz（器件上限 33MHz 之内） */
    s.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    s.SPI_Mode              = SPI_Mode_Master;
    s.SPI_DataSize          = SPI_DataSize_8b;
    s.SPI_CPOL              = SPI_CPOL_Low;
    s.SPI_CPHA              = SPI_CPHA_1Edge;
    s.SPI_NSS               = SPI_NSS_Soft;
    s.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    s.SPI_FirstBit          = SPI_FirstBit_MSB;
    s.SPI_CRCPolynomial     = 7;
    SPI_Init(SPI2, &s);
    SPI_Cmd(SPI2, ENABLE);

    return (W25Q64_ReadID() == W25Q64_JEDEC_ID) ? 1 : 0;
}

/** @brief 读取 JEDEC 厂商/容量 ID（0xEF4017） */
uint32_t W25Q64_ReadID(void)
{
    uint16_t id1, id2, id3;
    W25_CS_L();
    w25_spi_rw(0x9F);
    id1 = w25_spi_rw(0xFF);
    id2 = w25_spi_rw(0xFF);
    id3 = w25_spi_rw(0xFF);
    W25_CS_H();
    return ((uint32_t)id1 << 16) | ((uint32_t)id2 << 8) | id3;
}

/** @brief 从任意地址连续读取（可跨页，无需擦除） */
void W25Q64_Read(uint32_t addr, uint8_t *buf, uint16_t len)
{
    uint16_t i;
    W25_CS_L();
    w25_spi_rw(0x03);                        /* 读数据命令   */
    w25_spi_rw((addr >> 16) & 0xFF);         /* 24位地址高位 */
    w25_spi_rw((addr >> 8) & 0xFF);
    w25_spi_rw(addr & 0xFF);
    for (i = 0; i < len; i++)
        buf[i] = w25_spi_rw(0xFF);           /* 空发读出     */
    W25_CS_H();
}

/** @brief 擦除一个 4KB 扇区（擦后全 0xFF，写之前必须擦） */
void W25Q64_SectorErase(uint32_t addr)
{
    w25_wren();
    W25_CS_L();
    w25_spi_rw(0x20);
    w25_spi_rw((addr >> 16) & 0xFF);
    w25_spi_rw((addr >> 8) & 0xFF);
    w25_spi_rw(addr & 0xFF);
    W25_CS_H();
    w25_wait_busy();                         /* 擦除最长 400ms */
}

/**
 * @brief 页编程：向 addr 写入 len 字节（**任意长度、可跨页**）
 *
 * ★★ 为什么必须在这里自己按页拆分（2026-09-11 修，踩过实坑）
 *   W25Q64 的一条 0x02(Page Program) 指令**不能跨 256 字节页边界**。
 *   W25Q64 数据手册明确：若一次送入的数据超过本页剩余空间，
 *   芯片内部的 8 位地址计数器会**回卷到本页开头**，
 *   把刚写进去的数据覆盖掉，而且**不返回任何错误** —— 典型静默数据损坏。
 *
 *   本工程真实踩坑：卡表 s_cards = CARD_MAX(50) × sizeof(CardEntry)(6) = **300 字节**，
 *   一次 PageProgram 写 300 字节 ⇒ 第 257~300 字节回卷写到扇区偏移 0~43，
 *   正好把**最前面 8 个卡槽抹成 0**。而 card_next_slot 从 0 开始分配，
 *   所以新卡刚录入就被自己抹掉：**表现为"能录入、界面也显示，一断电就丢失"**
 *   （RAM 里还在，所以当场看是好的，极具迷惑性）。
 *
 *   现在统一按页拆分，调用者不必再关心页边界。
 *   前提不变：目标扇区必须**先擦除**（本工程由 W25Q64_SectorErase 负责）。
 */
void W25Q64_PageProgram(uint32_t addr, const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint16_t n;                              /* 本次（不满一页时）要写的字节数 */

    while (len)
    {
        /* 本页剩余可写字节数 = 256 - (addr % 256)；超过则只写满本页就停 */
        n = (uint16_t)(256u - (addr & 0xFFu));
        if (n > len)
            n = len;

        w25_wren();                          /* 每条编程指令前都要重新写使能 */
        W25_CS_L();
        w25_spi_rw(0x02);
        w25_spi_rw((addr >> 16) & 0xFF);
        w25_spi_rw((addr >> 8) & 0xFF);
        w25_spi_rw(addr & 0xFF);
        for (i = 0; i < n; i++)
            w25_spi_rw(buf[i]);
        W25_CS_H();
        w25_wait_busy();                     /* 页编程典型 0.7ms，必须等完再下一页 */

        addr += n;                           /* 推进到下一页 */
        buf  += n;
        len  = (uint16_t)(len - n);
    }
}
