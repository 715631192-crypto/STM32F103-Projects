#include "i2c_soft.h"
#include "delay.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

/* ================================================================
 *  I2C 总线互斥锁（★ 解决"屏幕文字错乱"的关键）
 *
 *  SSD1306(OLED) 与 DS3231(RTC) 共用同一条软件 I2C 总线(PB8/PB9)，
 *  但访问它们的任务与优先级都不同：
 *      access_task  (prio 4) —— 每轮都调 board_unix_time() 读 DS3231
 *      network_task (prio 2) —— 拼 MQTT token 的 et 时读 DS3231
 *      input_task   (prio 3) —— 键盘回显写 OLED
 *      ui_task      (prio 1) —— 刷整屏 OLED
 *
 *  高优先级任务会在低优先级任务"一次 I2C 事务传到一半"时把它抢断，
 *  两条事务的 START / 数据位 / STOP 交织在一起，从机就会把半个字节
 *  当成数据收下 → 屏幕随机花屏、文字错乱（DS3231 也可能偶发读到错值）。
 *  这在本工程"时钟从软时钟切到真实 DS3231"之后才暴露：软时钟不碰总线。
 *
 *  解决：把锁下沉到"事务"这一层 —— 任一事务(START..STOP)开始即独占总线，
 *  事务内部不可被抢占。用递归锁是因为上层 oled.c 自己也持有递归锁，
 *  会嵌套进入这里。
 * ================================================================ */
static SemaphoreHandle_t s_i2c_mtx = NULL;

static void i2c_lock(void)
{
    /* 调度器还没跑起来时是单线程，无需加锁；此时锁也可能尚未创建 */
    if (s_i2c_mtx && xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        xSemaphoreTakeRecursive(s_i2c_mtx, portMAX_DELAY);
}

static void i2c_unlock(void)
{
    if (s_i2c_mtx && xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        xSemaphoreGiveRecursive(s_i2c_mtx);
}

/* ---------------- SDA 方向切换 ---------------- */

/** @brief SDA 切为输出（发数据时） */
static void sda_out(void)
{
    GPIO_InitTypeDef g;
    g.GPIO_Pin   = GPIO_Pin_9;
    g.GPIO_Mode  = GPIO_Mode_Out_PP;     /* 推挽输出 */
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &g);
}

/** @brief SDA 切为输入（读应答/读数据时），释放总线让从机拉低 */
static void sda_in(void)
{
    GPIO_InitTypeDef g;
    g.GPIO_Pin   = GPIO_Pin_9;
    g.GPIO_Mode  = GPIO_Mode_IPU;        /* 上拉输入 */
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &g);
}

/** @brief GPIO 初始化：PB8/PB9 均推挽输出高电平（总线空闲态） */
void IIC_Init(void)
{
    GPIO_InitTypeDef g;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    g.GPIO_Pin   = GPIO_Pin_8 | GPIO_Pin_9;
    g.GPIO_Mode  = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &g);

    IIC_SCL_H();// 初始化时 SCL 高电平
    IIC_SDA_H();// 初始化时 SDA 高电平

    /* 创建总线互斥锁（递归）：调度器启动前创建一次，全局唯一。
     * 放在 IIC_Init 里是因为它一定先于 OLED_Init / DS3231_Init 执行。 */
    if (s_i2c_mtx == NULL)
        s_i2c_mtx = xSemaphoreCreateRecursiveMutex();
}

/* ---------------- I2C 时序基本单元 ----------------
 * 每个电平间约 2~4us，总线速率约 100kHz
 */

/** @brief 起始信号：SCL 高时 SDA 由高变低 */
static void iic_start(void)
{
    sda_out();
    IIC_SDA_H(); IIC_SCL_H(); delay_us(4);
    IIC_SDA_L();               delay_us(4);   /* SDA 下降沿 = START */
    IIC_SCL_L();               delay_us(4);   /* 钳住总线，准备传数据 */
}

/** @brief 停止信号：SCL 高时 SDA 由低变高 */
static void iic_stop(void)
{
    sda_out();
    IIC_SCL_L(); IIC_SDA_L(); delay_us(4);
    IIC_SCL_H();               delay_us(4);
    IIC_SDA_H();               delay_us(4);   /* SDA 上升沿 = STOP */
}

/**
 * @brief 等待从机应答
 * @return 1 = 收到 ACK；0 = 无应答（总线异常/器件不存在）
 */
static uint8_t iic_wait_ack(void)
{
    uint8_t t = 0;
    sda_in();// 切换为输入模式
    IIC_SDA_H(); delay_us(1);
    IIC_SCL_H(); delay_us(1);
    while (IIC_SDA_READ())              /* SDA 被从机拉低 = ACK */
    {
        if (++t > 250)
        {
            iic_stop();
            return 0;                   /* 超时无应答 */
        }
    }
    IIC_SCL_L();                        /* SCL 拉低，结束此位 */
    return 1;
}

/** @brief 主机产生 ACK（继续读） */
static void iic_ack(void)
{
    IIC_SCL_L(); sda_out();
    IIC_SDA_L(); delay_us(5);
    IIC_SCL_H(); delay_us(5);
    IIC_SCL_L();
}

/** @brief 主机产生 NACK（最后一字节读完后） */
static void iic_nack(void)
{
    IIC_SCL_L(); sda_out();
    IIC_SDA_H(); delay_us(5);
    IIC_SCL_H(); delay_us(5);
    IIC_SCL_L();
}

/** @brief 发送一个字节（MSB 在前） */
static void iic_send_byte(uint8_t b)
{
    uint8_t i;
    sda_out();
    IIC_SCL_L();
    for (i = 0; i < 8; i++)
    {
        if (b & 0x80) IIC_SDA_H(); else IIC_SDA_L();
        b <<= 1;
        delay_us(5);
        IIC_SCL_H();                    /* SCL 高电平期间数据有效 */
        delay_us(5);
        IIC_SCL_L();                    /* 下降沿后可更新下一位 */
    }
}

/** @brief 接收一个字节，ack=1 回 ACK，ack=0 回 NACK */
static uint8_t iic_read_byte(uint8_t ack)
{
    uint8_t i, b = 0;
    sda_in();
    for (i = 0; i < 8; i++)
    {
        IIC_SCL_L(); delay_us(5);
        IIC_SCL_H();
        b <<= 1;
        if (IIC_SDA_READ()) b |= 0x01;
        delay_us(5);
    }
    IIC_SCL_L();
    if (ack) iic_ack(); else iic_nack();
    return b;
}

/* ---------------- 面向器件的读写接口 ---------------- */

/**
 * @brief 无寄存器地址的连续写（OLED 专用：data[0]=控制字节 0x00/0x40）
 * @return 1 成功 0 失败（无应答）
 */
uint8_t IIC_WriteBytesRaw(uint8_t dev_w, const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint8_t  ok = 1;

    i2c_lock();                         /* ★ 整个事务独占总线 */
    iic_start();
    iic_send_byte(dev_w);               /* 器件地址(写) */
    if (!iic_wait_ack()) { iic_stop(); ok = 0; }
    else
    {
        for (i = 0; i < len; i++)
        {
            iic_send_byte(data[i]);
            if (!iic_wait_ack()) { iic_stop(); ok = 0; break; }
        }
    }
    i2c_unlock();
    return ok;
}

/**
 * @brief 读寄存器（DS3231 用）：先写寄存器地址，重复起始后连读 len 字节
 *        读到的最后一字节回 NACK，随后 STOP
 */
uint8_t IIC_ReadRegs(uint8_t dev_w, uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint8_t  ok = 1;

    i2c_lock();                         /* ★ 整个事务独占总线 */
    iic_start();
    iic_send_byte(dev_w);
    if (!iic_wait_ack()) { iic_stop(); ok = 0; }
    else
    {
        iic_send_byte(reg);                 /* 寄存器地址 */
        if (!iic_wait_ack()) { iic_stop(); ok = 0; }
        else
        {
            iic_start();                        /* Repeated START 转为读方向 */
            iic_send_byte(dev_w | 0x01);        /* 器件地址(读) */
            if (!iic_wait_ack()) { iic_stop(); ok = 0; }
            else
            {
                for (i = 0; i < len; i++)
                    buf[i] = iic_read_byte(i < (len - 1) ? 1 : 0);
                iic_stop();
            }
        }
    }
    i2c_unlock();
    return ok;
}

/** @brief 写单个寄存器（DS3231 设置时间用） */
uint8_t IIC_WriteReg(uint8_t dev_w, uint8_t reg, uint8_t val)
{
    uint8_t ok = 1;

    i2c_lock();                         /* ★ 整个事务独占总线 */
    iic_start();
    iic_send_byte(dev_w);
    if (!iic_wait_ack()) { iic_stop(); ok = 0; }
    else
    {
        iic_send_byte(reg);
        if (!iic_wait_ack()) { iic_stop(); ok = 0; }
        else
        {
            iic_send_byte(val);
            if (!iic_wait_ack()) { iic_stop(); ok = 0; }
            else iic_stop();
        }
    }
    i2c_unlock();
    return ok;
}
