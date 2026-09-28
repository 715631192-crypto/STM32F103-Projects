//
// Created by 11946 on 2026/8/8.
//
#include "app_ps2.h"

/* ============================================================================
 *  PS2手柄 → 指令映射 (app_ps2.c) —— 红灯模式控制舵机, 绿灯模式控制底盘
 * ----------------------------------------------------------------------------
 *  每个表项格式:  <PS2_REDxx:按下命令^松开命令>
 *    ^ 之前 = 按键"按下"时执行的命令
 *    ^ 之后 = 按键"松开"时执行的命令(通常是 #xxxPDST! 原地停止)
 *    L2/R2 → 5号舵机, L1/R1 → 4号舵机, 方向键 → 0~3号舵机, SE/ST → 全部复位
 *  绿灯模式(pre_cmd_set_grn)发 $DCR:四轮差速! 控制底盘, 不走舵机。
 *  注意: 按键产生的命令会被"伪装"成串口收到的指令, 写入 uart_receive_buf 并置
 *        uart1_get_ok=1, 然后走和真实串口完全相同的 app_usart_run() 管道(见下)。
 * ========================================================================== */

const char* pre_cmd_set_red[PSX_BUTTON_NUM] = {
    // 手柄按键功能字符串 红灯模式下使用
    "<PS2_RED01:#005P0600T2000!^#005PDST!>", //L2
    "<PS2_RED02:#005P2400T2000!^#005PDST!>", //R2 
    "<PS2_RED03:#004P0600T2000!^#004PDST!>", //L1
    "<PS2_RED04:#004P2400T2000!^#004PDST!>", //R1 
    "<PS2_RED05:#002P2400T2000!^#002PDST!>", //RU
    "<PS2_RED06:#003P0600T2000!^#003PDST!>", //RR
    "<PS2_RED07:#002P0600T2000!^#002PDST!>", //RD
    "<PS2_RED08:#003P2400T2000!^#003PDST!>", //RL
    "<PS2_RED09:$DJR!>", // SE
    "<PS2_RED10:>", // AL
    "<PS2_RED11:>", // AR
    "<PS2_RED12:$DJR!>", // ST
    "<PS2_RED13:#001P0600T2000!^#001PDST!>", //LU
    "<PS2_RED14:#000P0600T2000!^#000PDST!>", //LR
    "<PS2_RED15:#001P2400T2000!^#001PDST!>", //LD
    "<PS2_RED16:#000P2400T2000!^#000PDST!>", //LL
};
const char* pre_cmd_set_grn[PSX_BUTTON_NUM] = {
    // 绿灯模式下按键的配置
    "<PS2_RED01:$DCR:0,500,500,0!^$DCR:0,0,0,0!>", //L2 左上500
    "<PS2_RED02:$DCR:500,0,0,500!^$DCR:0,0,0,0!>", //R2 右上500
    "<PS2_RED03:$DCR:0,1000,1000,0!^$DCR:0,0,0,0!>", //L1 左上1000
    "<PS2_RED04:$DCR:1000,0,0,1000!^$DCR:0,0,0,0!>", //R2 右上1000
    "<PS2_RED05:$DCR:1000,1000,1000,1000!^$DCR:0,0,0,0!>", //RU 前进1000
    "<PS2_RED06:$DCR:1000,-1000,-1000,1000!^$DCR:0,0,0,0!>", //RR 右平移1000
    "<PS2_RED07:$DCR:-1000,-1000,-1000,-1000!^$DCR:0,0,0,0!>", //RD 后退1000
    "<PS2_RED08:$DCR:-1000,1000,1000,-1000!^$DCR:0,0,0,0!>", //RL 左平移1000
    "<PS2_RED09:$DJR!>", //SE
    "<PS2_RED10:>", //AL
    "<PS2_RED11:>", //AR
    "<PS2_RED12:$DJR!>", //ST
    "<PS2_RED13:$DCR:500,500,500,500!^$DCR:0,0,0,0!>", //LU 前进500
    "<PS2_RED14:$DCR:500,-500,500,-500!^$DCR:0,0,0,0!>", //LR 右转500
    "<PS2_RED15:$DCR:-500,-500,-500,-500!^$DCR:0,0,0,0!>", //LD 后退500
    "<PS2_RED16:$DCR:-500,500,-500,500!^$DCR:0,0,0,0!>", //LL 左转500
};

/**
 * PS2设备控制初始化
 */
void app_ps2_init(void)
{
    ps2_init();
}

/**
 * 循环执行任务
 */
void app_ps2_run(void)
{
    static u8 psx_button_bak[2] = {0};//按键状态缓存
    static u32 systick_ms_bak = 0;
    // 每50ms处理1次
    if (SysTick_get_ms() - systick_ms_bak < 50)
        return;
    systick_ms_bak = SysTick_get_ms();
    ps2_write_read(); // 读取ps2数据
    // 0=ff 1=41 2=5a 3=ff 4=ff 5=ff 6=ff 7=ff 8=ff 绿灯
    // 0=ff 1=73 2=5a 3=ff 4=ff 5=7f 6=80 7=7f 8=80 红灯
    // usart_printf(USART_DEBUG, "\r\n 0=%x,1=%x,2=%x,3=%x,4=%x,5=%x,6=%x,7=%x,8=%x",
    //              psx_buf[0], psx_buf[1], psx_buf[2], psx_buf[3], psx_buf[4], psx_buf[5], psx_buf[6], psx_buf[7],
    //              psx_buf[8]);
    // 应答字节为0x5A才表示手柄就绪，未就绪时跳过避免误触发按键
    if (psx_buf[2] != 0x5A)
    {
        return;
    }
    // 对比两次获取的按键值是否相同，相同就不处理，不同则处理
    if ((psx_button_bak[0] != psx_buf[3]) || (psx_button_bak[1] != psx_buf[4]))
    {
        // 处理buf3和buf4两个字节，这两个字节存储着手柄16个按键的状态
        parse_psx_buf(psx_buf + 3, psx_buf[1]);
        psx_button_bak[0] = psx_buf[3];
        psx_button_bak[1] = psx_buf[4];
    }
}

/* PS2 协议中 psx_buf[3]=SELECT/START/D-Pad, psx_buf[4]=L2/R2/L1/R1/形状键。
 * temp = (psx_buf[3] << 8) | psx_buf[4]
 *   temp bit 0..7  = psx_buf[4] = L2,R2,L1,R1,RU,RR,RD,RL → 对应数组下标 0..7
 *   temp bit 8..15 = psx_buf[3] = SE,AL,AR,ST,LU,LR,LD,LL → 对应数组下标 8..15
 * 所以 temp bit 编号与 pre_cmd_set_xxx[] 数组下标一一对应，直接用 i 即可。*/

/**
 * 处理手柄按键字符
 * @param buf  = psx_buf + 3, buf[0]=按键字节0(L2..RL), buf[1]=按键字节1(SE..LL)
 * @param mode PS2_LED_RED(0x73) 或 PS2_LED_GRN(0x41)
 */
void parse_psx_buf(u8* buf, u8 mode)
{
    u8 i, pos, cmd_has_dollar, cmd_has_hash;//循环变量, 命令位置, 是否包含$和#
    static u16 bak = 0xFFFF;//上一次按键状态(初值0xFFFF=全1, 表示"所有键都松开过")
    u16 temp, press_mask, release_mask; //当前按键状态, 按下掩码, 松开掩码
    const char** p_pre_cmd_set;//命令字符串指针(指向红灯或绿灯按键表)
    size_t slen;

    // buf[0]=buf3, buf[1]=buf4。拼成16bit: bit0~7=buf[0](L2..RL), bit8~15=buf[1](SE..LL)
    temp = ((u16)buf[0] << 8) | (u16)buf[1];//当前按键状态(注意: PS2按键低有效, 0=按下, 1=松开)
    if (temp == bak) return;//如果当前按键状态与上一次完全相同(没有键变化), 直接跳过, 不重复处理

    // 根据手柄回传的模式字节选择对应的按键映射表
    if (mode == PS2_LED_RED)       p_pre_cmd_set = (const char**)pre_cmd_set_red;
    else if (mode == PS2_LED_GRN)  p_pre_cmd_set = (const char**)pre_cmd_set_grn;
    else                           { bak = temp; return; } // 未知模式(如未就绪)直接退出

    // 按键低有效：1=松开 0=按下。用两次状态的异或思想算出"这次发生了什么边沿"
    // bak  = 上次状态, temp = 本次状态
    // press_mask   = 上次是1(松开) 且 本次是0(按下) 的位 = 刚按下的键
    press_mask   =  bak & ~temp;    // 松开→按下
    // release_mask = 上次是0(按下) 且 本次是1(松开) 的位 = 刚松开的键
    release_mask = ~bak &  temp;    // 按下→松开
    bak = temp; // 更新为本次状态, 供下次比较

    // 遍历16个按键(对应 pre_cmd_set_xxx[] 的 0~15 下标)。注意: 一次状态变化可能同时有多个键(很少见, 一般逐个处理)
    for (i = 0; i < 16; ++i)//遍历16个按键
    {
        u16 bitmask = (u16)(1U << i);//第 i 位的掩码(0000 0001 ... 1000 0000 0000 0000)
        // 如果该键这次既没"刚按下"也没"刚松开", 跳过(只处理发生边沿变化的键)
        if ((press_mask & bitmask) == 0 && (release_mask & bitmask) == 0)//如果当前按键未按下也未松开，跳过
            continue;

        /* temp bit 编号与数组下标一一对应，直接用 i */
        memset(uart_receive_buf, 0, sizeof(uart_receive_buf));//清空接收缓冲区
        strncpy((char*)uart_receive_buf, p_pre_cmd_set[i], UART_BUF_SIZE - 1);//把整条表项复制进缓冲, 形如 "<PS2_RED01:#005P0600T2000!^#005PDST!>"

        if (press_mask & bitmask)//如果当前按键是"刚按下"
        {
            /* 按下事件：取 ^ 之前的命令（无 ^ 则取整条，去掉末尾的 >） */
            pos = str_contain_str(uart_receive_buf, (u8*)"^");//查找^位置
            if (pos)
            {
                // 有 ^ : 把 ^ 处截成结束符, 只保留 "按下命令"。注意 pos 指向 ^, pos-1 正好把 ^ 前的 '!' 保留
                uart_receive_buf[pos - 1] = '\0';
            }
            else//如果没有^(如 SE/ST 的 "<PS2_RED09:$DJR!>"), 去掉末尾的 >
            {
                slen = strlen((char*)uart_receive_buf);
                if (slen > 0 && uart_receive_buf[slen - 1] == '>')
                    uart_receive_buf[slen - 1] = '\0';
            }

            // ★关键点: uart_receive_buf 前面有 11 字节固定前缀 "<PS2_REDxx:" (尖括号+PS2_RED+两位序号+冒号)
            // 所以用 uart_receive_buf + 11 跳过前缀, 指向真正的命令起始('#' 或 '$')
            cmd_has_dollar = str_contain_str(uart_receive_buf + 11, (u8*)"$");//查找$位置
            cmd_has_hash   = str_contain_str(uart_receive_buf + 11, (u8*)"#");//查找#位置
            if (cmd_has_dollar || cmd_has_hash)//如果包含$或#(确实是舵机/系统命令)
            {
                uart1_close();      // 临时关闭串口接收中断, 防止写缓冲时被真实串口数据打断
                uart1_get_ok = 0;
                // 把"去掉前缀后的纯命令"(从+11偏移起)拷到 cmd_return, 再拷回 uart_receive_buf → 完成"剥离标签前缀"
                strcpy((char*)cmd_return, (char*)uart_receive_buf + 11);
                strcpy((char*)uart_receive_buf, (char*)cmd_return);
                uart1_get_ok = 1;   // 置位: 告诉主循环"有一条指令就绪待处理"
                uart1_open();       // 重新打开串口接收
                /* 命令($xxx!) → mode=1 (parse_cmd), 舵机(#xxx!) → mode=2 (parse_action) */
                // 这一步就是"伪装成串口": 让手柄按键产生的指令和电脑串口发来的走完全相同的 app_usart_run() 管道
                uart1_mode = cmd_has_dollar ? (u8)1 : (u8)2;//根据是否包含$或#设置模式
            }
        }
        else if (release_mask & bitmask)//如果当前按键是"刚松开"
        {
            /* 松开事件：取 ^ 之后的命令(通常是 #xxxPDST! 原地停止) */
            pos = str_contain_str(uart_receive_buf, (u8*)"^");//查找^位置
            if (pos == 0) continue; // 没有 ^ (如 SE/ST) 就不处理松开事件

            // 松开命令在 ^ 之后, 所以这次从 uart_receive_buf + pos(指向 ^ 之后) 开始找 $ / #
            cmd_has_dollar = str_contain_str((u8*)uart_receive_buf + pos, (u8*)"$");//查找$位置
            cmd_has_hash   = str_contain_str((u8*)uart_receive_buf + pos, (u8*)"#");//查找#位置
            if (!cmd_has_dollar && !cmd_has_hash) continue; // 后面不是命令就不处理

            uart1_close();
            uart1_get_ok = 0;
            // 直接把 ^ 之后的内容(从 +pos 起, 已跳过 "<PS2_REDxx:" 和 按下命令)拷出
            strcpy((char*)cmd_return, (char*)uart_receive_buf + pos);
            slen = strlen((char*)cmd_return);
            if (slen > 0 && cmd_return[slen - 1] == '>')
                cmd_return[slen - 1] = '\0'; // 去掉末尾 '>'
            strcpy((char*)uart_receive_buf, (char*)cmd_return);
            uart1_get_ok = 1;
            uart1_open();
            // 同样"伪装成串口": 松开命令($ 或 #)也经 app_usart_run() 管道处理
            uart1_mode = cmd_has_dollar ? (u8)1 : (u8)2;
        }
    }

    BEEP_ON();
    Delay_ms(10);
    BEEP_OFF();
}
