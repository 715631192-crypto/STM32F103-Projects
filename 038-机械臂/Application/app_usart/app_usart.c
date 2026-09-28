#include "./app_usart/app_usart.h"
//初始化app_usart
void app_usart_init(void)
{
    uart1_init(115200);
    uart3_init(115200);
    Usartprintf(USART_DEBUG, "UART13_INIT SUCCESS\r\n");
}

// 循环检测串口接收到的指令
/**
 * @函数描述: 循环检测串口接收到的指令
 * @return {*}
 */
void app_usart_run(void) {
    if (uart1_get_ok) {
        // printf("\r\n app_uart_run = %s \r\n", uart_receive_buf);
        if (uart1_mode == 1) {
            // 命令模式
            parse_cmd(uart_receive_buf);
        }
        else if (uart1_mode == 2) {
            // 单个舵机调试
            parse_action(uart_receive_buf);
        }
        else if (uart1_mode == 3) {
            // 多路舵机调试
            parse_action(uart_receive_buf);
        }
        else if (uart1_mode == 4) {
            // 存储模式
            save_action(uart_receive_buf);
        }
        else if (uart1_mode == 5) {
            // 转换角度模式: S005,90D (0~270度线性映射 P500~2500)
            parse_angle(uart_receive_buf);
        }

        uart1_mode = 0;
        uart1_get_ok = 0;
    }
}
