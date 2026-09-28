#include "mfrc522.h"
#include "delay.h"
#include <stdio.h>          /* 临时调试打印用（RC522_TRACE） */
#include <string.h>         /* memset：清收发缓冲，避免回读栈垃圾 */

/*
 * 引脚分配（SPI1 硬件接口 + 软件 CS）：
 *   PA4 = 片选 NSS(CS)  PA5 = 时钟 SCK  PA6 = 主入从出 MISO  PA7 = 主出从入 MOSI
 * MFRC522 的 SPI 时序：CPOL=0 CPHA=0（模式0），MSB 在前，时钟 ≤10MHz
 */

#define RC522_CS_H()  GPIO_SetBits(GPIOA, GPIO_Pin_4)
#define RC522_CS_L()  GPIO_ResetBits(GPIOA, GPIO_Pin_4)

/* 临时调试开关：=1 时把每一次寻卡的 TRANCEIVE 内部快照打到串口。
 * 用途：区分"数字域通、射频域不通"的几种可能。定位完改回 0。 */
#define RC522_TRACE   0

/* ---------------- MFRC522 寄存器地址 ---------------- */
#define     CommandReg          0x01
#define     CommIEnReg          0x02
#define     DivIEnReg           0x03
#define     ComIrqReg           0x04
#define     DivIrqReg           0x05
#define     ErrorReg            0x06
#define     Status1Reg          0x07
#define     Status2Reg          0x08
#define     FIFODataReg         0x09
#define     FIFOLevelReg        0x0A
#define     ControlReg          0x0C
#define     BitFramingReg       0x0D
#define     CollReg             0x0E
#define     ModeReg             0x11
#define     TxModeReg           0x12
#define     RxModeReg           0x13
#define     TxControlReg        0x14
#define     TxAutoReg           0x15
#define     RFCfgReg            0x26   /* 接收增益（RxGain[6:4]） */
#define     TModeReg            0x2A
#define     TPrescalerReg       0x2B
#define     TReloadRegL         0x2C
#define     TReloadRegH         0x2D
#define     VersionReg          0x37

/* ---------------- MFRC522 命令字 ---------------- */
#define PCD_IDLE              0x00
#define PCD_AUTH              0x0E
#define PCD_RECEIVE           0x08
#define PCD_TRANSMIT          0x04
#define PCD_TRANSCEIVE        0x0C
#define PCD_RESETPHASE        0x0F
#define PCD_CALCCRC           0x03

/* ---------------- MIFARE 卡命令字 ---------------- */
/* PICC_REQIDL / PICC_ANTICOLL1 已上移到 mfrc522.h 供上层使用 */

/* ---------------- 底层寄存器读写 ----------------
 * 地址帧格式：bit7=读/写(1读0写)，bit6 恒 1，bit0 恒 0
 */

static void rc522_write(uint8_t addr, uint8_t val)
{
    RC522_CS_L();
    SPI_I2S_SendData(SPI1, (addr << 1) & 0x7E);   /* 地址(写) */
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    SPI_I2S_ReceiveData(SPI1);
    SPI_I2S_SendData(SPI1, val);                  /* 数据 */
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    SPI_I2S_ReceiveData(SPI1);
    RC522_CS_H();
}

static uint8_t rc522_read(uint8_t addr)
{
    uint8_t val;
    RC522_CS_L();
    SPI_I2S_SendData(SPI1, ((addr << 1) & 0x7E) | 0x80);  /* 地址(读) */
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    SPI_I2S_ReceiveData(SPI1);
    SPI_I2S_SendData(SPI1, 0x00);                 /* 空读，产出数据 */
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    val = (uint8_t)SPI_I2S_ReceiveData(SPI1);
    RC522_CS_H();
    return val;
}

static void rc522_set_bitmask(uint8_t reg, uint8_t mask)
{
    rc522_write(reg, rc522_read(reg) | mask);
}

static void rc522_clear_bitmask(uint8_t reg, uint8_t mask)
{
    rc522_write(reg, rc522_read(reg) & (~mask));
}

/* ---------------- 初始化 ---------------- */

/**
 * @brief SPI1 + MFRC522 复位与基础配置
 */
void MFRC522_Init(void)
{
    GPIO_InitTypeDef  g;
    SPI_InitTypeDef   s;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);

    /* PA4 = CS 推挽输出 */
    g.GPIO_Pin   = GPIO_Pin_4;
    g.GPIO_Mode  = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &g);
    RC522_CS_H();

    /* PA5/PA6/PA7 = SCK/MISO/MOSI 复用 */
    g.GPIO_Pin  = GPIO_Pin_5 | GPIO_Pin_7;
    g.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &g);
    g.GPIO_Pin  = GPIO_Pin_6;
    g.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &g);

    /* SPI1 主模式：72MHz/16 = 4.5MHz（< 10MHz 上限） */
    s.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    s.SPI_Mode              = SPI_Mode_Master;
    s.SPI_DataSize          = SPI_DataSize_8b;
    s.SPI_CPOL              = SPI_CPOL_Low;      /* 模式 0 */
    s.SPI_CPHA              = SPI_CPHA_1Edge;
    s.SPI_NSS               = SPI_NSS_Soft;
    s.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;
    s.SPI_FirstBit          = SPI_FirstBit_MSB;
    s.SPI_CRCPolynomial     = 7;
    SPI_Init(SPI1, &s);
    SPI_Cmd(SPI1, ENABLE);

    /* ---- 复位 ----
     * RC522 没有上电自动清状态机：MCU 复位后经常起不来（MISO 无驱动 / Version 0x00），
     * 旧代码只能整机断电重上电救回。本板 PC14 已接 RC522 的 RST（见 sensor.c），
     * 这里先给 RST 一个低脉冲做**硬复位**，把内部状态机彻底清零；随后再走一次
     * 软复位（PowerDown 位 + PCD_RESETPHASE）双保险。这样每次初始化都能自愈，
     * 不再依赖断电重上电。 */
    {
        GPIO_InitTypeDef r;
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
        r.GPIO_Pin   = GPIO_Pin_14;          /* PC14 = RC522 RST */
        r.GPIO_Mode  = GPIO_Mode_Out_PP;
        r.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(GPIOC, &r);
        GPIO_ResetBits(GPIOC, GPIO_Pin_14); /* RST 拉低，进入硬复位 */
        delay_ms(2);
        GPIO_SetBits(GPIOC, GPIO_Pin_14);    /* RST 释放 */
        delay_ms(2);
    }
    rc522_set_bitmask(CommandReg, 0x10);     /* PowerDown = 1 */
    delay_ms(2);
    rc522_clear_bitmask(CommandReg, 0x10);   /* PowerDown = 0，释放 */
    delay_ms(2);
    rc522_write(CommandReg, PCD_RESETPHASE);
    delay_ms(5);                             /* 等内部振荡器起振（手册要求 ≥1ms） */

    /* 定时器/发射参数（MFRC522 参考配置） */
    rc522_write(TModeReg, 0x8D);          /* 定时器自动启动           */
    rc522_write(TPrescalerReg, 0x3E);     /* 分频                     */
    /* ★ 2026-09-15 读取可靠性优化①：把"无卡等待超时"从 30(≈25ms) 缩到 10(≈8ms)。
     * 这个超时只在**天线区没卡**时才会走完（有卡时 ATQA 约 1ms 内就回），
     * 25ms 的等待纯属白等：轮询一次就阻塞输入任务 25ms，导致"想快也快不起来"。
     * 缩到 ≈8ms 后，把轮询周期从 300ms 提到 100ms 反而更省 CPU。
     * 8ms 仍远大于 ATQA 往返（<1ms），不影响边缘弱耦合卡的应答。 */
    rc522_write(TReloadRegL, 10);
    rc522_write(TReloadRegH, 0);
    rc522_write(TxAutoReg, 0x40);         /* 100% ASK 调制            */
    rc522_write(ModeReg, 0x3D);           /* CRC 初值 0x6363          */
    /* ★ 2026-09-15 读取可靠性优化②：接收增益拉满。
     * RFCfgReg 复位值 0x48（RxGain=33dB），弱耦合/远距离时卡的回包解调不稳，
     * 表现就是"时灵时不灵"。写 0x70（RxGain=48dB，寄存器允许的最大值）
     * 显著提高读到的概率。门锁场景同一时刻只有一张卡，无需担心多卡增益问题。 */
    rc522_write(RFCfgReg, 0x70);

    MFRC522_AntennaOn();
}

/** @brief 打开天线（TxControlReg 的两个 TX 电源位） */
void MFRC522_AntennaOn(void)
{
    rc522_set_bitmask(TxControlReg, 0x03);
}

/* ---------------- 链路自检 ---------------- */

/**
 * @brief SPI 双向通路自检
 *
 * 思路：MFRC522_Init() 里已经往 TModeReg 写了 0x8D、往 TPrescalerReg 写了 0x3E，
 * 这两个都是可读写寄存器，回读应当原值返回。用**两个不同的字节图案**分别验证，
 * 比只读一个版本寄存器可靠得多：
 *   - 两值都对   → SPI 的写与读都在工作，模块活着（问题在别处：天线/卡片/线圈）
 *   - 读回 0x00  → MISO 被拉死为低（MISO 短路/接错，或模块没供电）
 *   - 读回 0xFF  → MISO 悬空、没人驱动（MISO 没接、模块没起来、片选没生效）
 * VersionReg(0x37) 是只读 ID，兼容片五花八门，所以只作参考、不作判据。
 */
uint8_t MFRC522_Check(void)
{
    if (rc522_read(TModeReg)      != 0x8D) return 0;
    if (rc522_read(TPrescalerReg) != 0x3E) return 0;
    return 1;
}

/** @brief 读版本寄存器（仅用于打印，不作为通断判据） */
uint8_t MFRC522_Version(void)
{
    return rc522_read(VersionReg);
}

/* ---------------- 与卡片通信 ---------------- */

/**
 * @brief 基础收发函数：向 FIFO 写入数据并执行命令
 * @param Command    PCD_TRANSCEIVE（收发一体）
 * @param InData     发送缓冲
 * @param InLenByte  发送字节数
 * @param OutData    接收缓冲
 * @param OutLenBit  输出：收到的位数
 */
static char rc522_com(uint8_t Command, uint8_t *InData, uint8_t InLenByte,
                      uint8_t *OutData, uint16_t *OutLenBit)
{
    char    status = MI_ERR;
    uint8_t irqEn  = 0x00;
    uint8_t waitFor = 0x00;
    uint8_t lastBits;
    uint8_t n;
    uint16_t i;

    if (Command == PCD_AUTH)       { irqEn = 0x12;  waitFor = 0x10; }
    if (Command == PCD_TRANSCEIVE) { irqEn = 0x77;  waitFor = 0x30; }

    rc522_write(CommIEnReg, (uint8_t)(irqEn | 0x80));  /* 开对应中断 */
    rc522_clear_bitmask(ComIrqReg, 0x80);              /* 清 IRQ 位 */
    rc522_set_bitmask(FIFOLevelReg, 0x80);             /* 清 FIFO    */
    rc522_write(CommandReg, PCD_IDLE);                 /* 取消当前命令 */

    for (i = 0; i < InLenByte; i++)                    /* 数据入 FIFO */
        rc522_write(FIFODataReg, InData[i]);

    rc522_write(CommandReg, Command);                  /* 执行命令 */
    if (Command == PCD_TRANSCEIVE)
        rc522_set_bitmask(BitFramingReg, 0x80);        /* 启动发送 */

    /* 轮询等待命令完成（定时器到或收到数据即置位） */
    i = 2000;
    do
    {
        n = rc522_read(ComIrqReg);
        i--;
    } while ((i != 0) && !(n & 0x01) && !(n & waitFor));

    rc522_clear_bitmask(BitFramingReg, 0x80);

    if (i != 0)                                        /* 未超时 */
    {
        if (!(rc522_read(ErrorReg) & 0x1B))            /* 无协议错误 */
        {
            status = MI_OK;
            if (n & irqEn & 0x01)                      /* 定时器到 = 无卡 */
                status = MI_NOTAGERR;
            if (Command == PCD_TRANSCEIVE)
            {
                n = rc522_read(FIFOLevelReg);          /* FIFO 中字节数 */
                lastBits = rc522_read(ControlReg) & 0x07;
                *OutLenBit = (lastBits) ? ((n - 1) * 8 + lastBits) : (n * 8);
                if (n == 0)   n = 1;
                if (n > MAXRLEN) n = MAXRLEN;
                for (i = 0; i < n; i++)
                    OutData[i] = rc522_read(FIFODataReg);
            }
        }
    }
    return status;
}

/**
 * @brief 让卡回到 IDLE 态（PICC_HALT）
 *
 * 读完 UID 不作 HALT 的话，卡停在 ACTIVE 态、不再应答 REQA：
 * 用户把卡一直压在读头上反而"读不到第二次"，必须拿开重放 ——
 * 软件上表现为"时灵时不灵"。这里主动 HALT，卡立刻可被再次寻到。
 * 无卡/已 HALT 时命令失败属正常，忽略返回值。
 */
void MFRC522_Halt(void)
{
    uint8_t  buf[4];
    uint16_t unLen;

    rc522_clear_bitmask(Status2Reg, 0x08);
    buf[0] = 0x50;                       /* PICC_HALT */
    buf[1] = 0x00;
    (void)rc522_com(PCD_TRANSCEIVE, buf, 2, buf, &unLen);
}

/**
 * @brief 寻卡：天线范围内寻找一张卡
 * @param req_code  PICC_REQIDL(寻未休眠卡)
 * @param tag_type  输出：卡类型（0x04 00 = Mifare S50）
 */
char MFRC522_Request(uint8_t req_code, uint8_t *tag_type)
{
    char    status;
    uint16_t unLen;
    uint8_t  buf[MAXRLEN];
#if RC522_TRACE
    uint8_t  tx_on;                          /* 收发前回读的天线控制字 */
#endif

    memset(buf, 0, sizeof(buf));             /* 必须先清零：rc522_com 在"一个字节都
                                              * 没收到"时只写 buf[0]，buf[1] 会是
                                              * 栈垃圾，被当成 ATQA 传出去/打印出来 */
    rc522_clear_bitmask(Status2Reg, 0x08);   /* 清加密标志 */
    rc522_write(BitFramingReg, 0x07);        /* 发送 7 位   */
    rc522_set_bitmask(TxControlReg, 0x03);   /* 确保天线开 */

#if RC522_TRACE
    tx_on = rc522_read(TxControlReg);        /* 刚置过位就回读，bit0/1 必须是 1 */
#endif

    buf[0] = req_code;
    status = rc522_com(PCD_TRANSCEIVE, buf, 1, buf, &unLen);

#if RC522_TRACE
    /* 读寄存器不会清中断标志，所以这里读到的就是命令结束时的快照：
     *   irq   ComIrqReg: 0x01=TimerIRQ(定时器到，天线区无卡) ← 只有它才会让 st=1
     *                    0x20/0x30=RxIRQ(收到了数据)
     *                    0x40=TxIRQ(发射完成)   0x00=什么都没发生
     *   err  错误寄存器 ErrorReg：0x01=协议错 ProtocolErr  0x02=奇偶错 ParityErr  0x04=CRC 错 CRCErr
     *                    0x08=冲突错 CollErr      0x10=缓冲区溢出 BufferOvfl
     *   fifo  FIFOLevelReg: 收到几个字节（正常路径下已被读空，恒为 0；
     *                        只有"轮询超时"那条路径没读 FIFO，才可能非 0）
     *   tx_on / tx_off: 命令前后各读一次 TxControlReg。
     *                    tx_on 为 0     → 天线位根本写不进去（复位没完成）
     *                    tx_on 有、tx_off 没有 → 芯片在收发过程中把天线关掉了
     *                    两次都有 bit0/1  → 天线一直是开的，真是"场上没卡" */
    printf("[RC522] tr st=%d unLen=%u irq=0x%02X err=0x%02X fifo=%u tx_on=0x%02X tx_off=0x%02X data=%02X%02X\r\n",
           (int)status, (unsigned)unLen,
           rc522_read(ComIrqReg), rc522_read(ErrorReg),
           rc522_read(FIFOLevelReg), tx_on, rc522_read(TxControlReg),
           buf[0], buf[1]);
#endif

    if ((status != MI_OK) || (unLen != 0x10))
        status = MI_ERR;

    tag_type[0] = buf[0];
    tag_type[1] = buf[1];
    return status;
}

/**
 * @brief 防冲撞：获取卡片 4 字节序列号(UID)，尾字节为 XOR 校验
 * @param snr  输出：4 字节卡号
 */
char MFRC522_Anticoll(uint8_t *snr)
{
    char    status;
    uint8_t i, snr_check = 0;
    uint16_t unLen;
    uint8_t  buf[MAXRLEN];

    rc522_clear_bitmask(Status2Reg, 0x08);
    rc522_write(BitFramingReg, 0x00);        /* 完整字节发送 */

    buf[0] = PICC_ANTICOLL1;                 /* 防冲撞命令 */
    buf[1] = 0x20;
    status = rc522_com(PCD_TRANSCEIVE, buf, 2, buf, &unLen);

    if (status == MI_OK)
    {
        for (i = 0; i < 4; i++)              /* 前 4 字节是 UID */
        {
            snr[i]     = buf[i];
            snr_check ^= buf[i];             /* XOR 校验 */
        }
        if (snr_check != buf[4])             /* 第 5 字节校验不符 */
            status = MI_ERR;
    }
    return status;
}
