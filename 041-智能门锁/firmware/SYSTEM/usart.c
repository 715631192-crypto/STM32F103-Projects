#include "usart.h"

/* ---------------- 环形缓冲区定义 ----------------
 * head：写入位置(中断内更新)  tail：读出位置(主循环更新)
 * 缓冲满时丢弃新字节（保住旧数据），缓冲空时读出返回 0
 */
typedef struct
{
    volatile uint16_t head;
    volatile uint16_t tail;
    uint8_t buf[1];   /* 柔性数组，实际长度由调用处传入 */
} RingBuf;

/* 三个串口各自的接收缓冲 */
static uint8_t  rx1_buf[USART1_RX_SIZE];
static uint16_t rx1_head = 0, rx1_tail = 0;
static uint8_t  rx2_buf[USART2_RX_SIZE];
static uint16_t rx2_head = 0, rx2_tail = 0;
static uint8_t  rx3_buf[USART3_RX_SIZE];
static uint16_t rx3_head = 0, rx3_tail = 0;

/* ---------------- 内部工具函数 ---------------- */

/** @brief 向环形缓冲写入 1 字节（中断上下文调用） */
static void ring_push(uint8_t *buf, uint16_t size,
                      volatile uint16_t *head, volatile uint16_t *tail, uint8_t b)
{
    uint16_t next = (*head + 1) % size;
    if (next != *tail)          /* 未满才写入，满则丢弃 */
    {
        buf[*head] = b;
        *head = next;
    }
}

/** @brief 从环形缓冲读出 1 字节，空则返回 0 */
static uint8_t ring_pop(uint8_t *buf, uint16_t size,
                        volatile uint16_t *head, volatile uint16_t *tail, uint8_t *out)
{
    if (*tail == *head)
        return 0;               /* 空 */
    *out = buf[*tail];
    *tail = (*tail + 1) % size;
    return 1;
}

/* ---------------- 初始化 ---------------- */

/** @brief 通用串口初始化：GPIO + 外设 + 中断（三个串口共用逻辑） */
static void usart_gpio_common_init(void)
{
    /* 各串口的 GPIO 时钟在下面单独使能，这里保留空位方便扩展 */
}

/**
 * @brief USART1 初始化（PA9=TX → ESP8266，PA10=RX ← ESP8266）
 * @param baud 波特率，ESP8266 AT 固件默认 115200
 */
void usart1_init(uint32_t baud)
{
    GPIO_InitTypeDef  g;
    USART_InitTypeDef u;
    NVIC_InitTypeDef  n;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

    /* PA9 复用推挽输出(TX) */
    g.GPIO_Pin   = GPIO_Pin_9;
    g.GPIO_Mode  = GPIO_Mode_AF_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &g);

    /* PA10 浮空输入(RX) */
    g.GPIO_Pin  = GPIO_Pin_10;
    g.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &g);

    u.USART_BaudRate            = baud;
    u.USART_WordLength          = USART_WordLength_8b;
    u.USART_StopBits            = USART_StopBits_1;
    u.USART_Parity              = USART_Parity_No;
    u.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    u.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART1, &u);

    /* 开接收中断 */
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    n.NVIC_IRQChannel                   = USART1_IRQn;
    n.NVIC_IRQChannelPreemptionPriority = 1;
    n.NVIC_IRQChannelSubPriority        = 1;
    n.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&n);

    USART_Cmd(USART1, ENABLE);
    (void)usart_gpio_common_init;
}

/**
 * @brief USART2 初始化（PA2=TX → 指纹模块，PA3=RX ← 指纹模块）
 * @param baud AS608/ZW101 默认 57600
 */
void usart2_init(uint32_t baud)
{
    GPIO_InitTypeDef  g;
    USART_InitTypeDef u;
    NVIC_InitTypeDef  n;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    g.GPIO_Pin   = GPIO_Pin_2;
    g.GPIO_Mode  = GPIO_Mode_AF_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &g);

    g.GPIO_Pin  = GPIO_Pin_3;
    g.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &g);

    u.USART_BaudRate            = baud;
    u.USART_WordLength          = USART_WordLength_8b;
    u.USART_StopBits            = USART_StopBits_1;
    u.USART_Parity              = USART_Parity_No;
    u.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    u.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART2, &u);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    n.NVIC_IRQChannel                   = USART2_IRQn;
    n.NVIC_IRQChannelPreemptionPriority = 1;
    n.NVIC_IRQChannelSubPriority        = 2;
    n.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&n);

    USART_Cmd(USART2, ENABLE);
}

/**
 * @brief USART3 初始化（PB10=TX，PB11=RX，调试打印口）
 */
void usart3_init(uint32_t baud)
{
    GPIO_InitTypeDef  g;
    USART_InitTypeDef u;
    NVIC_InitTypeDef  n;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    g.GPIO_Pin   = GPIO_Pin_10;
    g.GPIO_Mode  = GPIO_Mode_AF_PP;// 复用推挽输出(TX)
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &g);

    g.GPIO_Pin  = GPIO_Pin_11;
    g.GPIO_Mode = GPIO_Mode_IN_FLOATING;// 浮空输入(RX)
    GPIO_Init(GPIOB, &g);

    u.USART_BaudRate            = baud;
    u.USART_WordLength          = USART_WordLength_8b;
    u.USART_StopBits            = USART_StopBits_1;
    u.USART_Parity              = USART_Parity_No;
    u.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    u.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART3, &u);

    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE); // 开接收中断，用于调试打印口
    n.NVIC_IRQChannel                   = USART3_IRQn;
    n.NVIC_IRQChannelPreemptionPriority = 1;
    n.NVIC_IRQChannelSubPriority        = 3;
    n.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&n);

    USART_Cmd(USART3, ENABLE);
}

/* ---------------- 发送（阻塞式，适合小数据量） ---------------- */

void usart_send_byte(USART_TypeDef *usart, uint8_t b)
{
    USART_SendData(usart, b);
    while (USART_GetFlagStatus(usart, USART_FLAG_TXE) == RESET)
        ;                       /* 等待发送数据寄存器空 */
}

void usart_send_bytes(USART_TypeDef *usart, const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++)
        usart_send_byte(usart, buf[i]);
    while (USART_GetFlagStatus(usart, USART_FLAG_TC) == RESET)
        ;                       /* 等待最后一字节完全发出 */
}

void usart_send_string(USART_TypeDef *usart, const char *str)
{
    while (*str)
        usart_send_byte(usart, (uint8_t)*str++);
}

/* ---------------- 接收（读环形缓冲） ---------------- */

uint8_t usart_read_byte(USART_TypeDef *usart, uint8_t *out)
{
    if (usart == USART1) return ring_pop(rx1_buf, USART1_RX_SIZE, &rx1_head, &rx1_tail, out);
    if (usart == USART2) return ring_pop(rx2_buf, USART2_RX_SIZE, &rx2_head, &rx2_tail, out);
    if (usart == USART3) return ring_pop(rx3_buf, USART3_RX_SIZE, &rx3_head, &rx3_tail, out);
    return 0;
}

uint16_t usart_read_bytes(USART_TypeDef *usart, uint8_t *buf, uint16_t max)
{
    uint16_t n = 0;
    while (n < max && usart_read_byte(usart, &buf[n]))
        n++;
    return n;
}
// 获取接收缓冲区可用字节数
uint16_t usart_rx_available(USART_TypeDef *usart)
{
    if (usart == USART1) return (uint16_t)((rx1_head + USART1_RX_SIZE - rx1_tail) % USART1_RX_SIZE);
    if (usart == USART2) return (uint16_t)((rx2_head + USART2_RX_SIZE - rx2_tail) % USART2_RX_SIZE);
    if (usart == USART3) return (uint16_t)((rx3_head + USART3_RX_SIZE - rx3_tail) % USART3_RX_SIZE);
    return 0;
}
// 清空接收缓冲区
void usart_rx_clear(USART_TypeDef *usart)
{
    if (usart == USART1) rx1_tail = rx1_head;
    if (usart == USART2) rx2_tail = rx2_head;
    if (usart == USART3) rx3_tail = rx3_head;
}

/* ---------------- 接收中断入口（stm32f10x_it.c 调用） ---------------- */

void usart_rx_irq(USART_TypeDef *usart)
{
    if (USART_GetITStatus(usart, USART_IT_RXNE) != RESET)
    {
        uint8_t b = (uint8_t)USART_ReceiveData(usart);   /* 读 DR 自动清标志 */
        if      (usart == USART1) ring_push(rx1_buf, USART1_RX_SIZE, &rx1_head, &rx1_tail, b);
        else if (usart == USART2) ring_push(rx2_buf, USART2_RX_SIZE, &rx2_head, &rx2_tail, b);
        else if (usart == USART3) ring_push(rx3_buf, USART3_RX_SIZE, &rx3_head, &rx3_tail, b);
    }

    /* ORE 溢出兜底：高速连续收包时数据被覆盖会置 ORE 且同样触发中断，
     * 若不清除会反复进中断拖死 CPU。读 SR（上面 GetFlagStatus）+ 读 DR 即清。 */
    if (USART_GetFlagStatus(usart, USART_FLAG_ORE) != RESET)
    {
        (void)USART_ReceiveData(usart);   /* 丢弃溢出字节，仅清标志 */
    }
}

/* ---------------- printf 重定向到 USART3 ---------------- */
int fputc(int ch, FILE *f)
{
    (void)f;
    usart_send_byte(USART3, (uint8_t)ch);
    return ch;
}
