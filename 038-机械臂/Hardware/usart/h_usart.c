#include "./usart/h_usart.h"
uint8_t uart_receive_buf[UART_BUF_SIZE];
uint16_t uart1_get_ok;
uint8_t uart1_mode;

// 初始化串口1
void uart1_init(uint32_t baudrate) // 初始化串口1 上位机通信
{
    // 使能时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

    // GPIO初始化
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // 发送引脚推挽输出模式
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; // 接收引脚浮空输入模式
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    // USART初始化
    USART_InitTypeDef USART_InitStructure;
    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART1, &USART_InitStructure);

    // 使能接收中断
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    // 使能发送中断
    USART_ITConfig(USART1, USART_IT_TXE, DISABLE);

    // 中断初始化
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART1, ENABLE);
}

// 初始化串口3
void uart3_init(uint32_t baudrate) // 初始化串口3 与设备通信
{
    // 使能时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    // GPIO初始化
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    // USART初始化
    USART_InitTypeDef USART_InitStructure;
    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART3, &USART_InitStructure);

    // 使能接收中断
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
    // 使能发送中断
    USART_ITConfig(USART3, USART_IT_TXE, DISABLE);
    // 中断初始化
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART3, ENABLE);
}

// 发送字节
void Usart_SendByte(USART_TypeDef *USARTx, uint8_t byte)
{
    USART_SendData(USARTx, byte); // 发送字节
    while (USART_GetFlagStatus(USARTx, USART_FLAG_TXE) == RESET)
        ; // 等待发送完成
}

// 发送字符串
void Usart_Sendstring(USART_TypeDef *USARTx, uint8_t *str)
{
    while (*str)
    {
        USART_SendData(USARTx, *str++); // 发送字符串
        while (USART_GetFlagStatus(USARTx, USART_FLAG_TXE) == RESET)
            ; // 等待发送完成
    }
}

// 发送数字
void Usart_SendInt(USART_TypeDef *USARTx, int num)
{
    char str[20];
    sprintf((char *)str, "%d", num);
    Usart_Sendstring(USARTx, (uint8_t *)str);
}

// 发送格式化字符串
void Usartprintf(USART_TypeDef *USARTx, const char *fmt, ...)
{
    static uint8_t str[1024]; // static，不占栈
    va_list args;
    va_start(args, fmt);
    vsnprintf((char *)str, sizeof(str), fmt, args);
    va_end(args);
    Usart_Sendstring(USARTx, str);
}

// 重定向fputc函数
int fputc(int ch, FILE *f)
{
    USART_ClearFlag(USART1, USART_FLAG_TC); // 手动清除发送完成标志
    USART_SendData(USART1, (uint8_t)ch);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
        ;
    return ch;
}

/* ============================================================================
 *  串口接收中断 —— uart1_mode 状态机(整个协议分发的"总开关")
 * ----------------------------------------------------------------------------
 *  收到第一个字符就决定本帧属于哪种指令, 收到对应结束符置 uart1_get_ok=1:
 *    首字符   mode    帧格式例子              结束符   处理函数
 *      $       1      $DJR! $DST! ...         !       parse_cmd   (系统命令)
 *      #       2      #005P0600T2000!         !       parse_action(舵机移动, 同时转发USART3)
 *      {       3      {多路舵机}               }       parse_action(与mode2共用解析)
 *      <       4      <动作组>                 >       save_action (存Flash)
 *      S       5      S005,90D               D       parse_angle (角度模式: 0~270度→P500~2500)
 *  注意: USART1(电脑)和USART3(总线舵机回传)共用同一套 uart1_mode/uart_receive_buf,
 *        所以下面两条中断服务函数逻辑完全一致。
 * ========================================================================== */
// 串口一接收中断服务函数
void USART1_IRQHandler(void)
{
    uint8_t sbuf_bak;
    static uint16_t buf_index = 0;
    if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET)
    {
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
        sbuf_bak = USART_ReceiveData(USART1);
        if (uart1_get_ok)
        {
            return;
        }

        if (uart1_mode == 0)
        {
            if (sbuf_bak == '$')
            {
                uart1_mode = 1;
            }
            else if (sbuf_bak == '#')
            {
                uart1_mode = 2;
            }
            else if (sbuf_bak == '{')
            {
                uart1_mode = 3;
            }
            else if (sbuf_bak == '<')
            {
                uart1_mode = 4;
            }
            else if (sbuf_bak == 'S')
            {
                uart1_mode = 5;
            }
            buf_index = 0;
        }
        uart_receive_buf[buf_index++] = sbuf_bak;

        if ((uart1_mode == 1) && (sbuf_bak == '!'))
        {
            uart_receive_buf[buf_index] = '\0';
            uart1_get_ok = 1;
        }
        else if ((uart1_mode == 2) && (sbuf_bak == '!'))
        {
            uart_receive_buf[buf_index] = '\0';
            uart1_get_ok = 1;
        }
        else if ((uart1_mode == 3) && (sbuf_bak == '}'))
        {
            uart_receive_buf[buf_index] = '\0';
            uart1_get_ok = 1;
        }
        else if ((uart1_mode == 4) && (sbuf_bak == '>'))
        {
            uart_receive_buf[buf_index] = '\0';
            uart1_get_ok = 1;
        }
        else if ((uart1_mode == 5) && (sbuf_bak == 'D'))
        {
            uart_receive_buf[buf_index] = '\0';
            uart1_get_ok = 1;
        }
        if (buf_index >= UART_BUF_SIZE)
        {
            buf_index = 0;
        }
    }
}

void USART3_IRQHandler(void)
{
    uint8_t sbuf_bak;
    static uint16_t buf_index = 0;
    if (USART_GetITStatus(USART3, USART_IT_RXNE) == SET)
    {
        USART_ClearITPendingBit(USART3, USART_IT_RXNE);
        sbuf_bak = USART_ReceiveData(USART3);
        if (uart1_get_ok)
        {
            return;
        }

        if (uart1_mode == 0)
        {
            if (sbuf_bak == '$')
            {
                uart1_mode = 1;
            }
            else if (sbuf_bak == '#')
            {
                uart1_mode = 2;
            }
            else if (sbuf_bak == '{')
            {
                uart1_mode = 3;
            }
            else if (sbuf_bak == '<')
            {
                uart1_mode = 4;
            }
            else if (sbuf_bak == 'S')
            {
                uart1_mode = 5;
            }
            buf_index = 0;
        }
        uart_receive_buf[buf_index++] = sbuf_bak;
        if ((uart1_mode == 4) && (sbuf_bak == '>'))
        {
            uart_receive_buf[buf_index] = '\0';
            uart1_get_ok = 1;
        }
        else if ((uart1_mode == 1) && (sbuf_bak == '!'))
        {
            uart_receive_buf[buf_index] = '\0';
            uart1_get_ok = 1;
        }
        else if ((uart1_mode == 2) && (sbuf_bak == '!'))
        {
            uart_receive_buf[buf_index] = '\0';
            uart1_get_ok = 1;
        }
        else if ((uart1_mode == 3) && (sbuf_bak == '}'))
        {
            uart_receive_buf[buf_index] = '\0';
            uart1_get_ok = 1;
        }
        else if ((uart1_mode == 5) && (sbuf_bak == 'D'))
        {
            uart_receive_buf[buf_index] = '\0';
            uart1_get_ok = 1;
        }
        if (buf_index >= UART_BUF_SIZE)
        {
            buf_index = 0;
        }
    }
}
