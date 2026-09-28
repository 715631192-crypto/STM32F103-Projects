#include "./app_servo/app_servo.h"

/* ============================================================================
 *  舵机指令解析与执行 (app_servo.c) —— 系统的"指令大脑"
 * ----------------------------------------------------------------------------
 *  三类舵机相关指令都在这里落地:
 *    1) parse_action(): 处理 #IDPposTtime!(移动) / #IDPSCK±bias!(调偏差) / #IDPDST!(停止)
 *       关键: 第68行 zx_uart_send_str() 会把整条指令原样转发到 USART3(总线舵机口),
 *             同时只对 PWM舵机(index<DJ_NUM)做 duoji_doing_set() 写本地波形。
 *             总线舵机(4~5号, ID 4/5/254)只靠USART3转发, 不进TIM2波形。
 *    2) parse_angle(): "角度模式" S005,90D → 0~270度线性映射 P500~2500 后转发 #IDPposT2000!
 *    3) parse_cmd():   $开头系统命令(复位/停止/动作组/蜂鸣...)
 *  动作组存储 save_action() 把 <...> 存进W25Q64 Flash, 回放时再交给 parse_action。
 * ========================================================================== */


u8 cmd_return[CMD_RETURN_SIZE];
eeprom_info_t eeprom_info;

u8 group_do_ok = 1;
u8 AI_mode = 255;
int do_start_index;     // 动作组执行 起始序号
int do_time;            // 动作组执行 执行次数
int group_num_start;    // 动作组执行 起始序号
int group_num_end;      // 动作组执行 终止序号
int group_num_times;    // 动作组执行 起始变量
u32 action_time = 0;

//发送串口指令
void zx_uart_send_str(u8* str)
{
    uart1_get_ok = 1;
    Usart_Sendstring(USART1, str);
    Usart_Sendstring(USART3, str);
    uart1_get_ok = 0;
}

// 转换角度模式: 解析 S005,90D (如 S005,90D / S5,90D)
// 总线舵机位置范围 500~2500 = 0~270度, 转换后转发 #IDPposT2000! 到总线舵机口
void parse_angle(u8* buf)
{
    u8 id = 0;
    u16 angle = 0, pos;
    u8 i;
    u8 cmd[24];

    // 跳过首字符 'S'
    i = 1;
    // 读取舵机号 ID (直到 ',' 分隔符, 支持 1~3 位, 如 005 / 5)
    while (buf[i] && buf[i] != ',')
    {
        id = id * 10 + (buf[i] - '0');
        i++;
    }
    if (buf[i] == ',') i++;   // 跳过分隔符 ','
    // 读取角度 (直到结束符 'D')
    while (buf[i] && buf[i] != 'D')
    {
        angle = angle * 10 + (buf[i] - '0');
        i++;
    }
    // 限位: 角度 0~270度 (超过则钳到 270, 防止映射出界)
    if (angle > 270) angle = 270;
    // 角度 -> P值: 0度=P500, 270度=P2500, 线性映射并四舍五入 (+135 = 270/2 做就近取整)
    pos = 500 + (u16)((angle * 2000 + 135) / 270);

    sprintf((char*)cmd, "#%03dP%04dT2000!", id, pos);
    zx_uart_send_str(cmd); // 转发到 USART1(回显) + USART3(总线舵机)
}

// 处理 #000P1500T1000! 类似的字符串
void parse_action(u8* uart_receive_buf)
{
    u16 index, time, i = 0;//舵机号, 时间, 循环变量
    int bias, len;//偏差值, 长度
    float pwm;//PWM值

    zx_uart_send_str(uart_receive_buf); //将指令发给总线舵机控制总线舵机

    
    // ===== 分支1：偏差值调整命令 #xxxPSCK±bbb! =====
    // 帧格式(13字节): # [1][2][3]=舵机ID  P  S C K  [8]=±号  [9][10][11]=偏差值  !
    //   例: #005PSCK+030!  → 给5号舵机加 +30 偏差
    if (uart_receive_buf[0] == '#' && uart_receive_buf[4] == 'P' && uart_receive_buf[5] == 'S'
        && uart_receive_buf[6] == 'C' && uart_receive_buf[7] == 'K' && uart_receive_buf[12] == '!')
    {
        //调整偏差值
        // 解析3位舵机号 ID: [1]百位 [2]十位 [3]个位 (字符'0'转数字)
        index = (uart_receive_buf[1] - '0') * 100 + (uart_receive_buf[2] - '0') * 10 + (uart_receive_buf[3] - '0');
        // 解析3位偏差量: [9][10][11] (正数, 符号见[8])
        bias = (uart_receive_buf[9] - '0') * 100 + (uart_receive_buf[10] - '0') * 10 + (uart_receive_buf[11] - '0');

        // 安全校验: 偏差必须在 [-500, +500] 内, 且舵机号必须小于 DJ_NUM(防止数组越界)
        if ((bias >= -500) && (bias <= 500) && (index < DJ_NUM))
        {
            if (uart_receive_buf[8] == '+')
            {
                // 先减去旧偏差、再加上新偏差(相对调整, 不会因多次调用而累加错)
                duoji_doing[index].cur = duoji_doing[index].cur - eeprom_info.dj_bias_pwm[index] + bias;
                eeprom_info.dj_bias_pwm[index] = bias; // 记录新偏差
            }
            else if (uart_receive_buf[8] == '-')
            {
                duoji_doing[index].cur = duoji_doing[index].cur - eeprom_info.dj_bias_pwm[index] + bias;
                eeprom_info.dj_bias_pwm[index] = -bias; // 负号: 存为 -bias
            }
            duoji_doing[index].aim = duoji_doing[index].cur; // 目标=当前, 偏差调整后立即生效停在当前位
            duoji_doing[index].inc = 0.001; // 极小步进(让TIM2平滑接管)
            rewrite_eeprom(); //把eeprom_info写入到W25Q64_INFO_ADDR_SAVE_STR位置(掉电不丢偏差)
        }
    }
    else if (uart_receive_buf[0] == '#' && uart_receive_buf[4] == 'P' && uart_receive_buf[5] == 'D'
             && uart_receive_buf[6] == 'S' && uart_receive_buf[7] == 'T' && uart_receive_buf[8] == '!')
    {
        // ===== 分支2：原地停止命令 #xxxPDST! =====
        // 帧格式(9字节): # [1][2][3]=舵机ID  P  D S T  !
        // 例: #005PDST!  → 5号舵机在当前位置立刻停住(松手即停)
        // 注意特征字是 "PDST"(P-D-S-T), 之前曾误写成 "PSTT" 导致停止命令被当PWM指令解析, 舵机瞬跳
        //原地停止执行 #xxxPDST!
        // 解析3位舵机号 ID
        index = (uart_receive_buf[1] - '0') * 100 + (uart_receive_buf[2] - '0') * 10 + (uart_receive_buf[3] - '0');
        if (index < DJ_NUM)
        {
            // 单个舵机停止: 步进清零 + 目标设为当前位置 = 不再移动
            duoji_doing[index].inc = 0;
            duoji_doing[index].aim = duoji_doing[index].cur;
        }
        else if (index == 255)
        {
            // 广播停止: ID=255 表示全部舵机(0~DJ_NUM-1)一起原地停下
            for (index = 0; index < DJ_NUM; index++)
            {
                duoji_doing[index].inc = 0;
                duoji_doing[index].aim = duoji_doing[index].cur;
            }
        }
        return; // 停止命令处理完直接返回, 不再走下方通用PWM解析
    }

    //舵机执行
    len = strlen((char*)uart_receive_buf); // 获取串口接收数据的长度
    while (uart_receive_buf[i] && (len >= i))
    {
        if (uart_receive_buf[i] == '#')
        {
            index = 0;
            i++;
            while (uart_receive_buf[i] && uart_receive_buf[i] != 'P')
            {
                index = index * 10 + uart_receive_buf[i] - '0';
                i++;
            }
        }
        else if (uart_receive_buf[i] == 'P')
        {
            pwm = 0;
            i++;
            while (uart_receive_buf[i] && uart_receive_buf[i] != 'T')
            {
                pwm = pwm * 10 + uart_receive_buf[i] - '0';
                i++;
            }
        }
        else if (uart_receive_buf[i] == 'T')
        {
            time = 0;
            i++;
            while (uart_receive_buf[i] && uart_receive_buf[i] != '!')
            {
                time = time * 10 + uart_receive_buf[i] - '0';
                i++;
            }
            if (index < DJ_NUM) //只对PWM舵机叠加偏差并设置输出，255等总线舵机ID跳过(指令已由zx_uart_send_str原样转发)
            {
                pwm += eeprom_info.dj_bias_pwm[index]; //偏差值
                duoji_doing_set(index, pwm, time);
            }
        }
        else
        {
            i++;
        }
    }
}

// 动作组保存函数，只有用<>包含的字符串才能在此函数中进行解析
// <G0001#000P1500T1500!#001P2000T1500!#002P2000T1500!#003P0850T1500!#004P1500T1500!#005P1500T1500!>
void save_action(u8* str)
{
    s32 action_index = 0;
    group_do_ok = 1;/* 停止动作组 */

    // 预存命令处理
    spiFlashOn(1); //切换SPI和LED引脚状态
    Delay_ms(10);

    //取消预存储命令<$!>
    if (str[1] == '$' && str[2] == '!')
    {
        eeprom_info.pre_cmd[PRE_CMD_SIZE] = 0;
        rewrite_eeprom();
        zx_uart_send_str((u8*)"@CLEAR PRE_CMD OK!");
        return;
    }
    else if (str[1] == '$')
    {
        //设置开机动作组成功！@SET PRE_CMD OK!
        //<$G0000#000P1500T1500!#002P1500T1500!#003P1500T1500!#004P1500T1500!#005P1500T1500!#005P1500T1500!>
        memset(eeprom_info.pre_cmd, 0, sizeof(eeprom_info.pre_cmd));
        strcpy((char*)eeprom_info.pre_cmd, (char*)str + 1); // 对字符串进行复制
        eeprom_info.pre_cmd[strlen((char*)str) - 2] = '\0'; // 赋值字符0
        eeprom_info.pre_cmd[PRE_CMD_SIZE] = FLAG_VERIFY;
        rewrite_eeprom();
        zx_uart_send_str((u8*)"@SET PRE_CMD OK!");
        zx_uart_send_str((u8*)eeprom_info.pre_cmd);
        return;
    }

    // 获取动作组的组号如果不正确，或是第6个字符不是#则认为字符串错误
    action_index = (str[2] - '0') * 1000 + (str[3] - '0') * 100 + (str[4] - '0') * 10 + (str[5] - '0');
    //<<G0000#000P1500T1000!>
    if ((action_index == -1) || str[6] != '#')
    {
        Usartprintf(USART_DEBUG, "E");
        return;
    }

    if ((action_index * ACTION_SIZE % 4096) == 0)
    {
        w25x_erase_sector(action_index * ACTION_SIZE / 4096); //擦除一个扇区
    }

    // 把尖括号替换成大括号直接存储到存储芯片里面去，则在执行动作组的时候直接拿出来解析就可以了
    replace_char(str, '<', '{');
    replace_char(str, '>', '}');
    w25x_write(str, action_index * ACTION_SIZE, strlen((char*)str) + 1);

    //读取测试
    //Usartprintf(USART_DEBUG, "read test:");
    //memset(str, 0, sizeof((char *)str));
    //w25x_read(str, action_index * ACTION_SIZE, ACTION_SIZE);
    //Usartprintf(USART_DEBUG, str);

    // 反馈一个A告诉上位机我已经接收到了
    Usart_Sendstring(USART1, (u8*)"动作组保存成功\r\n");
    // Usart_Sendstring(USART3, (u8*)"A");
    spiFlashOn(0);
    return;
}

// 所有舵机停止命令:     $DST!
// 第 x 个舵机停止命令:  $DST:x!
// 单片机重启命令:       $RST!
// 检查动作组 x 到 y 组命令: $CGP:x-y!
// 执行第 x 个动作:     $DGS:x!
// 执行第 x 到 y 组动作 z 次: $DGT:x-y, z!  $DGT:0-1,1!
// 获取第 x 到 y 组动作:  $PTG:%x-y!  $PTG:0-1!
// 所有舵机复位命令:     $DJR!
// 获取应答信号:         $GETA!
// 设置开机动作命令:     $BOOT:x!  x=0关闭/1自定义/2挥手/3伸展(存EEPROM掉电不丢)
// */
void parse_cmd(u8* cmd)
{
    int pos, i, index, int1, int2;//解析命令用的变量，pos:命令位置，i:循环变量，index:舵机索引，int1:动作组索引，int2:动作组索引

    //Usartprintf(USART_DEBUG, cmd);
    if ((pos = str_contain_str(cmd, (u8*)"$DRS!"), pos))
    {
        //测试命令
        Usartprintf(USART_DEBUG, "hello word!");
    }
    else if ((pos = str_contain_str(cmd, (u8*)"$DST!"), pos))
    {
        //所有舵机停止命令
        group_do_ok = 1;
        for (i = 0; i < DJ_NUM; i++)
        {
            duoji_doing[i].inc = 0;
            duoji_doing[i].aim = duoji_doing[i].cur;
        }
        zx_uart_send_str((u8*)"#255PDST!"); // 总线停止
    }
    else if ((pos = str_contain_str(cmd, (u8*)"$DST:"), pos))
    {
        //第 x 个舵机停止命令
        // 越界检查: index 必须在 [0, DJ_NUM) 内, 否则 duoji_doing[index] 会写爆数组
        if (sscanf((char*)cmd, "$DST:%d!", &index) && index >= 0 && index < DJ_NUM)
        {
            duoji_doing[index].inc = 0;
            duoji_doing[index].aim = duoji_doing[index].cur;
            sprintf((char*)cmd_return, "#%03dPDST!\r\n", (int)(index));
            zx_uart_send_str(cmd_return);
            memset(cmd_return, 0, sizeof(cmd_return));
        }
    }
    else if ((pos = str_contain_str(cmd, (u8*)"$RST!"), pos))
    {
        //单片机复位
        soft_reset();
    }
    else if ((pos = str_contain_str(cmd, (u8*)"$DCR:"), pos))
    {
        //四路差速控制命令(手柄绿灯模式按键)，原样转发给外部控制板
        zx_uart_send_str(cmd);
    }
    else if ((pos = str_contain_str(cmd, (u8*)"$PTG:"), pos))
    {
        //获取动作组
        if (sscanf((char*)cmd, "$PTG:%d-%d!", &int1, &int2))
        {
            print_group(int1, int2);
        }
    }
    else if ((pos = str_contain_str(cmd, (u8*)"$DGS:"), pos))
    {
        //执行第 x 个动作
        if (sscanf((char*)cmd, "$DGS:%d!", &int1))
        {
            group_do_ok = 1;
            do_group_once(int1);
        }
    }
    else if ((pos = str_contain_str(cmd, (u8*)"$DGT:"), pos))
    {
        //执行第 x 到 y组动作 z次
        if (sscanf((char*)cmd, "$DGT:%d-%d,%d!", &group_num_start, &group_num_end, &group_num_times))
        {
            group_do_ok = 1;
            if (group_num_start != group_num_end)
            {
                do_start_index = group_num_start;
                do_time = group_num_times;
                group_do_ok = 0;
            }
            else
            {
                do_group_once(group_num_start);
            }
        }
    }
    else if ((pos = str_contain_str(cmd, (u8*)"$DJR!"), pos))
    {
        //所有舵机复位命令
        zx_uart_send_str((u8*)"#255P1500T2000!"); // 总线舵机广播复位中位(ID 255,位置1500)
        AI_mode = 255;
        for (i = 0; i < DJ_NUM; i++)
        {
            duoji_doing[i].aim = 1500 + eeprom_info.dj_bias_pwm[i];
            duoji_doing[i].time = 2000;
            duoji_doing[i].inc = (duoji_doing[i].aim - duoji_doing[i].cur) / (duoji_doing[i].time / 20.000);
        }
    }
    else if ((pos = str_contain_str(cmd, (u8*)"$GETA!"), pos))
    {
        //获取应答信号
        Usartprintf(USART_DEBUG, "AAA");
    }
    else if ((pos = str_contain_str(cmd, (u8*)"$BEEP!"), pos))
    {
        //蜂鸣器鸣叫时间
        beep_on_times(1, 100);
    }
    else if ((pos = str_contain_str(cmd, (u8*)"$BOOT:"), pos))
    {
        //设置开机动作: $BOOT:0!关闭 / 1=自定义pre_cmd / 2=挥手 / 3=伸展
        //存进EEPROM掉电不丢, 上电自动回放; 设完立即预览一次方便调参
        if (sscanf((char*)cmd, "$BOOT:%d!", &index) && index >= 0 && index <= 3)
        {
            eeprom_info.boot_sel = (u8)index;
            rewrite_eeprom(); // 把含 boot_sel 的整个 eeprom_info 写进 Flash
            sprintf((char*)cmd_return, "@SET BOOT=%d OK!\r\n", (int)index);
            zx_uart_send_str(cmd_return);
            memset(cmd_return, 0, sizeof(cmd_return));
            boot_action_run(); // 立即预览
        }
    }
}

// 动作组批量执行
void loop_action(void)
{
    static long long systick_ms_bak = 0; // 上次执行时间
    if (group_do_ok == 0) // 动作组未完成
    {
        if ((SysTick_get_ms() - systick_ms_bak) > action_time) // 动作时间到
        {
            systick_ms_bak = SysTick_get_ms();
            if ((group_num_times != 0) && (do_time == 0))
            {
                group_do_ok = 1;
                Usartprintf(USART_DEBUG, "@GroupDone!");
                return;
            }

            // 调用 do_start_index 个动作
            do_group_once(do_start_index);
            // 已播到本段末尾?
            if (do_start_index == group_num_end)
            {
                do_start_index = group_num_start; // 回到起点, 准备下一轮
                if (group_num_times != 0)
                {
                    do_time--;                   // 完成一轮, 次数-1
                }
            }
            else
            {
                // 正序(start<end)往后走, 倒序(start>end)往前走
                if (group_num_start < group_num_end)
                    do_start_index++;
                else
                    do_start_index--;
            }
        }
        else
        {
            action_time = 10; // 10ms 动作时间
        }
    }
}

// 把eeprom_info写入到W25Q64_INFO_ADDR_SAVE_STR位置
void rewrite_eeprom(void)
{
    spiFlashOn(1);
    Delay_ms(10);
    //擦除一个扇区 最少150毫秒
    w25x_erase_sector(W25Q64_INFO_ADDR_SAVE_STR / 4096);
    //写入整个扇区
    w25x_writeS((u8*)&eeprom_info, W25Q64_INFO_ADDR_SAVE_STR, sizeof(eeprom_info_t));
    spiFlashOn(0);
}

//字符串中的字符替代函数 把str字符串中所有的 ch1 换成 ch2
void replace_char(u8* str, u8 ch1, u8 ch2)
{
    while (*str)
    {
        if (*str == ch1)
        {
            *str = ch2;
        }
        str++;
    }
    return;
}

//打印存储在芯片里的动作组，从串口1中发送出来 $CGP:x-y!这个命令调用
void print_group(int start, int end)
{
    spiFlashOn(1);
    Delay_ms(10);
    if (start > end)
    {
        int_exchange(&start, &end);
    }
    for (; start <= end; start++)
    {
        memset(uart_receive_buf, 0, sizeof(uart_receive_buf));
        w25x_read(uart_receive_buf, start * ACTION_SIZE, ACTION_SIZE);
        Usart_Sendstring(USART1, uart_receive_buf);
        Usart_Sendstring(USART1, (u8*)"\r\n");
    }
    spiFlashOn(0);
}

//两个int变量交换
void int_exchange(int* int1, int* int2)
{
    int int_temp;
    int_temp = *int1;
    *int1 = *int2;
    *int2 = int_temp;
}

//判断子串
uint16_t str_contain_str(unsigned char* str, unsigned char* str2)
{
    unsigned char* str_temp, * str_temp2;
    str_temp = str;
    str_temp2 = str2;
    while (*str_temp)
    {
        if (*str_temp == *str_temp2)
        {
            while (*str_temp2)
            {
                if (*str_temp++ != *str_temp2++)
                {
                    str_temp = str_temp - (str_temp2 - str2)+1;
                    str_temp2 = str2;
                    break;
                }
            }
            if (!*str_temp2)
            {
                return (str_temp - str);
            }
           
        }
        else{
            str_temp++;
        }
       
    }
    return 0;
}

// 执行动作组1次，参数是动作组序号
void do_group_once(int group_num)
{
    spiFlashOn(1);
    Delay_ms(10);
    // 将uart_receive_buf 清零
    memset(uart_receive_buf, 0, sizeof(uart_receive_buf));
    // 从存储芯片中读取第 group_num 个动作组
    w25x_read(uart_receive_buf, group_num * ACTION_SIZE, ACTION_SIZE);
    // 获取最大的组时间
    action_time = getMaxTime(uart_receive_buf);
    // 把读取出来的动作组传递到 parse_action 执行
    parse_action(uart_receive_buf);
    spiFlashOn(0);
}

// 获取最大时间
int getMaxTime(u8* str)
{
    int i = 0, max_time = 0, tmp_time = 0;
    while (str[i])
    {
        if (str[i] == 'T')
        {
            // 从 T 后逐位读取数字, 直到 '!' 或非数字为止(兼容 3/4 位等不同长度, 避免误读)
            int j = i + 1;
            tmp_time = 0;
            while (str[j] && str[j] != '!' && str[j] >= '0' && str[j] <= '9')
            {
                tmp_time = tmp_time * 10 + (str[j] - '0');
                j++;
            }
            if (tmp_time > max_time)
            {
                max_time = tmp_time;
            }
            i = j; // 跳到 '!' 之后继续扫描下一条指令
            continue;
        }
        i++;
    }
    return max_time;
}

/* 单片机软件复位 */
void soft_reset(void)
{
    Usartprintf(USART1, "stm32 reset\r\n");
    __set_FAULTMASK(1); // 关闭所有中断
    NVIC_SystemReset(); // 复位
   
}

//============================================================================
// 开机启动动作(可切换) —— 新增功能
//----------------------------------------------------------------------------
// 上电时 main() 先 load_eeprom() 把 boot_sel 从Flash读回, 再在初始化末尾
// 调用 boot_action_run() 按 boot_sel 回放对应动作:
//   0=关闭(保持servo_init的中位)  1=自定义pre_cmd(用 <$G...> 存)  2=挥手  3=伸展
// 切换指令: $BOOT:x!  (x=0/1/2/3), 存EEPROM掉电不丢, 设完立即预览。
//============================================================================

// 上电从W25Q64把 eeprom_info 读回RAM(开机动作选择/偏差/自定义动作)
void load_eeprom(void)
{
    spiFlashOn(1);
    Delay_ms(10);
    w25x_read((u8*)&eeprom_info, W25Q64_INFO_ADDR_SAVE_STR, sizeof(eeprom_info_t));
    spiFlashOn(0);
    // 兼容: 旧固件没写 boot_sel 字段, 读到 0xFF 等非法值 → 强制关闭并清空, 保持上电安全(舵机停中位)
    if (eeprom_info.boot_sel > 3)
    {
        eeprom_info.boot_sel = 0;
        memset(eeprom_info.dj_bias_pwm, 0, sizeof(eeprom_info.dj_bias_pwm));
        memset(eeprom_info.pre_cmd, 0, sizeof(eeprom_info.pre_cmd));
    }
}

// 开机动作辅助: 发送一组舵机目标帧并等待(让TIM2把舵机平滑扫到位)
static void boot_pose(const char* frame, u16 wait_ms)
{
    parse_action((u8*)frame); // 内部转发到总线舵机口(USART3) + 设置本地PWM舵机(0~5)
    Delay_ms(wait_ms);
}

// 开机动作2: 挥手打招呼(抬起5号臂左右摆两下)
static void boot_wave(void)
{
    boot_pose("#255P1500T800!", 900);   // 总线舵机先回中
    boot_pose("#005P2000T500!", 600);   // 抬起5号臂
    boot_pose("#005P1000T350!", 450);   // 摆左
    boot_pose("#005P2000T350!", 450);   // 摆右
    boot_pose("#005P1000T350!", 450);   // 摆左
    boot_pose("#005P2000T350!", 450);   // 摆右
    boot_pose("#000P1500T600!#001P1500T600!#002P1500T600!#003P1500T600!#004P1500T600!#005P1500T600!", 900); // 全回中
}

// 开机动作3: 伸展舒展(双臂上举再缓缓落下)
static void boot_stretch(void)
{
    boot_pose("#255P1500T800!", 900);   // 总线舵机先回中
    boot_pose("#001P2000T800!#002P1000T800!#003P1000T800!", 1000); // 双臂上举舒展
    boot_pose("#001P1500T800!#002P1500T800!#003P1500T800!", 1200); // 缓缓落下
    boot_pose("#000P1500T600!#001P1500T600!#002P1500T600!#003P1500T600!#004P1500T600!#005P1500T600!", 900); // 全回中
}

// 按 eeprom_info.boot_sel 回放对应开机动作
void boot_action_run(void)
{
    u8 sel = eeprom_info.boot_sel;
    if (sel == 0) return; // 关闭: 保持 servo_init 的中位(已回中)
    else if (sel == 1)
    {
        // 自定义: 回放用户用 <$G...> 存的 pre_cmd 动作组帧
        if (eeprom_info.pre_cmd[PRE_CMD_SIZE] == FLAG_VERIFY && eeprom_info.pre_cmd[0] != '\0')
        {
            parse_action(eeprom_info.pre_cmd); // 驱动PWM舵机, 并转发总线舵机
        }
    }
    else if (sel == 2) boot_wave();    // 挥手打招呼
    else if (sel == 3) boot_stretch(); // 伸展舒展
}

