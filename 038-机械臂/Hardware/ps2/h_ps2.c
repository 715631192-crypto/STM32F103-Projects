#include "./ps2/h_ps2.h"

/* ============================================================================
 *  PS2手柄驱动 (h_ps2.c) —— 用GPIO模拟SPI读按键
 * ----------------------------------------------------------------------------
 *  手柄通信: 9字节一帧, 主机发命令、手柄回数据(STM32这里用GPIO位翻转模拟SPI):
 *    psx_buf[0]=0xFF 起始  [1]=模式字节(红灯0x73/绿灯0x41)  [2]=0x5A应答(就绪标志)
 *    [3][4]=16个按键状态(低有效: 0=按下)  [5~8]=两个摇杆模拟量
 *  时序要点: 必须微秒级延时(用SysTick精确Delay_us), DAT脚必须上拉输入(开漏)。
 *  上电初始化序列: 唤醒→进配置模式(0x43)→开红灯模拟模式(0x44)→退配置, 保证手柄默认红灯。
 * ========================================================================== */

u8 psx_buf[9] = {0};

void ps2_init(void)
{
  // 重新配置SWJ禁用：PA13/14/15(PS2的DAT/CMD/CS)默认是JTAG引脚，
  // rcc_Init()中RCC_DeInit()会关闭AFIO时钟，这里再次使能并确认释放为普通GPIO
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
  GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);

  // 初始化时钟
  RCC_APB2PeriphClockCmd(PS2_DAT_GPIO_CLK | PS2_CMD_GPIO_CLK | PS2_CLK_GPIO_CLK | PS2_CS_GPIO_CLK, ENABLE);

  // 初始化PS2接口
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.GPIO_Pin = PS2_DAT_PIN;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU; //PS2数据线为开漏输出，主机必须上拉输入才能读到正确电平
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(PS2_DAT_GPIO_PORT, &GPIO_InitStruct);
  GPIO_InitStruct.GPIO_Pin = PS2_CMD_PIN;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(PS2_CMD_GPIO_PORT, &GPIO_InitStruct);
  GPIO_InitStruct.GPIO_Pin = PS2_CLK_PIN;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(PS2_CLK_GPIO_PORT, &GPIO_InitStruct);
  GPIO_InitStruct.GPIO_Pin = PS2_CS_PIN;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(PS2_CS_GPIO_PORT, &GPIO_InitStruct);
  PS2_CS(1);
  PS2_CLK(1);
  PS2_CMD(1);

  // 手柄初始化序列：唤醒→进入配置模式→红灯(模拟模式)→退出配置模式
  // 保证上电后手柄自动处于红灯模式，方向键映射到舵机控制表
  {
    u8 i, cmd[9];
    // 1. 短轮询唤醒手柄
    cmd[0] = 0x01; cmd[1] = 0x42;
    for (i = 2; i < 9; i++) cmd[i] = 0x00;
    PS2_CS(0);
    for (i = 0; i < 9; i++) ps2_transfer(cmd[i]);
    PS2_CS(1);
    Delay_ms(10);
    // 2. 进入配置模式
    cmd[0] = 0x01; cmd[1] = 0x43;
    for (i = 2; i < 9; i++) cmd[i] = 0x00;
    PS2_CS(0);
    for (i = 0; i < 9; i++) ps2_transfer(cmd[i]);
    PS2_CS(1);
    Delay_ms(10);
    // 3. 打开模拟模式(红灯)
    cmd[0] = 0x01; cmd[1] = 0x44; cmd[2] = 0x00; cmd[3] = 0x01; cmd[4] = 0x03;
    for (i = 5; i < 9; i++) cmd[i] = 0x00;
    PS2_CS(0);
    for (i = 0; i < 9; i++) ps2_transfer(cmd[i]);
    PS2_CS(1);
    Delay_ms(10);
    // 4. 退出配置模式
    cmd[0] = 0x01; cmd[1] = 0x43;
    for (i = 2; i < 9; i++) cmd[i] = 0x00;
    PS2_CS(0);
    for (i = 0; i < 9; i++) ps2_transfer(cmd[i]);
    PS2_CS(1);
  }
}


// 读写一个字节
u8 ps2_transfer(unsigned char dat)
{
  unsigned char rd_data = 0;
  unsigned char wt_data = dat;
  u8 i;
  for (i = 0; i < 8; i++)
  {
    // 步骤1：输出当前bit到CMD
    PS2_CMD(wt_data & (1 << i));
    Delay_us(3);
    // 步骤2：时钟拉低，产生下降沿发送数据
    PS2_CLK(0);
    Delay_us(6);                                                          
    // 步骤3：CLK低电平稳定读取DAT
    if (PS2_DAT())
    {
      rd_data |= (1 << i);
    }
    // 步骤4：时钟拉高，结束本次bit传输
    PS2_CLK(1);
    Delay_us(3);
  }
  return rd_data;
}

void ps2_write_read(void)
{
  PS2_CS(0);
  psx_buf[0] = ps2_transfer(START_CMD);
  psx_buf[1] = ps2_transfer(ASK_DAT_CMD);
  psx_buf[2] = ps2_transfer(0x00);
  psx_buf[3] = ps2_transfer(0x00);
  psx_buf[4] = ps2_transfer(0x00);
  psx_buf[5] = ps2_transfer(0x00);
  psx_buf[6] = ps2_transfer(0x00);
  psx_buf[7] = ps2_transfer(0x00);
  psx_buf[8] = ps2_transfer(0x00);
  PS2_CS(1);
}
