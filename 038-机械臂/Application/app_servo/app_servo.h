#ifndef __APP_SERVO_H__
#define __APP_SERVO_H__

#include "main.h"

#define DJ_NUM 8 //舵机数量
#define W25Q64_INFO_ADDR_SAVE_STR (((8 << 10) - 4) << 10) //(8*1024‑4)*1024，eeprom_info 结构体存储的位置
#define FLAG_VERIFY 0x25 // 校验标志
#define ACTION_SIZE 256 // 一个动作的存储大小
#define PRE_CMD_SIZE 128 // 预命令大小
#define CMD_RETURN_SIZE 1024 // 保存命令大小

extern u8 cmd_return[CMD_RETURN_SIZE]; //定义命令数组大小

//存储命令结构体
typedef struct {
    u32 version;         //版本号
    u32 dj_record_num;   //舵机记录编号
    u8 pre_cmd[PRE_CMD_SIZE + 1]; //预存命令数组
    int dj_bias_pwm[DJ_NUM + 1];  //舵机偏差数组
    u8 boot_sel;                  //开机动作选择: 0=关闭(保持中位) 1=自定义pre_cmd 2=挥手 3=伸展
} eeprom_info_t;

extern eeprom_info_t eeprom_info;

void parse_action(u8* uart_receive_buf);  //执行舵机命令
void parse_angle(u8* buf);                //转换角度模式 S,ID,角度D -> #IDPposT2000!
void save_action(u8* str);                //存储舵机命令
void parse_cmd(u8* cmd);                  //执行命令模式
void loop_action(void);                   //动作组批量执行
void rewrite_eeprom(void);                //写入W25Q64存储位置
void replace_char(u8* str, u8 ch1, u8 ch2); //字符替代

void int_exchange(int* int1, int* int2);  //int变量交换
void print_group(int start, int end);      //打印动作组
uint16_t str_contain_str(unsigned char* str, unsigned char* str2); //判断子串
void do_group_once(int group_num);         //执行动作组1次，参数是动作组序号
int getMaxTime(u8* str);                   //获取最大时间
void soft_reset(void);                     //单片机软件复位
void load_eeprom(void);                    //上电从Flash读回eeprom_info(开机动作/偏差/自定义动作)
void boot_action_run(void);                //按 boot_sel 回放对应开机动作

#endif // __APP_SERVO_H__


