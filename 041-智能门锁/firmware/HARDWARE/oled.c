#include "oled.h"
#include "oledfont.h"
#include "oledfont_cn.h"       /* 16 像素行高中文（汉字 16x16 + ASCII 8x16） */
#include "i2c_soft.h"
#include "delay.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* 实际使用的 OLED 8 位写地址（初始化时在 0x78/0x7A 间自动探测） */
static uint8_t s_oled_addr = OLED_ADDR;

/* I2C 总线互斥锁：UI 任务/休眠/胁迫开锁等多处并发写 OLED，
 * 不加锁会导致 I2C 事务交错、屏幕花屏。用递归锁，因 ShowString→ShowChar→SetPos 嵌套 */
static SemaphoreHandle_t s_oled_mtx = NULL;

/* ★ 必须用 __inline，不能写 inline：
 *   Keil AC5 在本工程是 C89 模式（uC99=0），C89 里 `inline` 不是关键字，
 *   会被当成普通标识符 → 报 #260-D(explicit type is missing) + #65(expected a ";")，
 *   后面整片代码解析错乱。__inline 是 AC5 扩展关键字，C89/C99 都认识。 */
static __inline void oled_lock(void)
{
    if (s_oled_mtx && xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        xSemaphoreTakeRecursive(s_oled_mtx, portMAX_DELAY);
}
static __inline void oled_unlock(void)
{
    if (s_oled_mtx && xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        xSemaphoreGiveRecursive(s_oled_mtx);
}

/* ---------------- 底层写入 ---------------- */

/** @brief 向 OLED 写 1 字节（命令或数据），成功返回 1 */
static uint8_t oled_wr_byte(uint8_t dat, uint8_t ctrl)
{
    uint8_t buf[2];
    buf[0] = ctrl;                     /* 控制字节：0x00命令 / 0x40数据 */
    buf[1] = dat;
    return IIC_WriteBytesRaw(s_oled_addr, buf, 2);
}

/** @brief 设置写入位置：x=列(0~127)，y=页(0~7) */
void OLED_SetPos(uint8_t x, uint8_t y)
{
    oled_wr_byte(0xB0 + y, OLED_CMD);            /* 页地址       */
    oled_wr_byte(((x & 0xF0) >> 4) | 0x10, OLED_CMD); /* 列高4位 */
    oled_wr_byte(x & 0x0F, OLED_CMD);            /* 列低4位       */
}

/* ---------------- 初始化 ---------------- */

/**
 * @brief SSD1306 上电初始化序列（标准 128x64 配置）
 *        含上电延时与 0x78/0x7A 地址自动探测，兼容 SA0 接高的模块。
 */
void OLED_Init(void)
{
    /* 上电稳定延时（SSD1306 VCC 稳定后需等待，用 DWT delay_us，调度器未启动也安全） */
    delay_us(50000);                  /* 50ms */

    /* ---- 地址探测：先发 0xAE 关显示命令，哪个地址应答就用哪个 ---- */
    s_oled_addr = OLED_ADDR;          /* 0x78 (SA0=GND) */
    if (!oled_wr_byte(0xAE, OLED_CMD))
    {
        s_oled_addr = OLED_ADDR | 0x02;  /* 0x7A (SA0=VCC) */
        oled_wr_byte(0xAE, OLED_CMD);
    }

    oled_wr_byte(0xAE, OLED_CMD);   /* 关显示（配置期间）        */
    oled_wr_byte(0xD5, OLED_CMD);   /* 设置显示时钟分频比/振荡器 */
    oled_wr_byte(0x80, OLED_CMD);   /* 默认分频                  */
    oled_wr_byte(0xA8, OLED_CMD);   /* 设置多路复用率            */
    oled_wr_byte(0x3F, OLED_CMD);   /* 1/64 占空比（64行）       */
    oled_wr_byte(0xD3, OLED_CMD);   /* 设置显示偏移              */
    oled_wr_byte(0x00, OLED_CMD);   /* 无偏移                    */
    oled_wr_byte(0x40, OLED_CMD);   /* 起始行 = 0                */
    oled_wr_byte(0x8D, OLED_CMD);   /* 电荷泵设置                */
    oled_wr_byte(0x14, OLED_CMD);   /* 开电荷泵（模块必需）      */
    oled_wr_byte(0x20, OLED_CMD);   /* 设置寻址模式              */
    oled_wr_byte(0x02, OLED_CMD);   /* 页寻址模式（默认）        */
    oled_wr_byte(0xA1, OLED_CMD);   /* 段重定义：左右镜像        */
    oled_wr_byte(0xC8, OLED_CMD);   /* COM 扫描方向：上下正常    */
    oled_wr_byte(0xDA, OLED_CMD);   /* COM 引脚硬件配置          */
    oled_wr_byte(0x12, OLED_CMD);   /* 交替 COM 配置             */
    oled_wr_byte(0x81, OLED_CMD);   /* 设置对比度                */
    oled_wr_byte(0x8F, OLED_CMD);   /* 对比度值（适中，避免过亮）*/
    oled_wr_byte(0xD9, OLED_CMD);   /* 设置预充电周期            */
    oled_wr_byte(0xF1, OLED_CMD);   /* 预充电周期                */
    oled_wr_byte(0xDB, OLED_CMD);   /* 设置 VCOMH 电压           */
    oled_wr_byte(0x40, OLED_CMD);   /* VCOMH                     */
    oled_wr_byte(0xA4, OLED_CMD);   /* 恢复显示内容（非全亮）    */
    oled_wr_byte(0xA6, OLED_CMD);   /* 正常显示（非反色）        */
    oled_wr_byte(0xAF, OLED_CMD);   /* 开显示                    */

    /* 创建递归互斥锁（ShowString→ShowChar→SetPos 嵌套调用需要递归） */
    if (s_oled_mtx == NULL)
        s_oled_mtx = xSemaphoreCreateRecursiveMutex();
}

void OLED_DisplayOn(void)
{
    oled_lock();
    oled_wr_byte(0x8D, OLED_CMD); oled_wr_byte(0x14, OLED_CMD); oled_wr_byte(0xAF, OLED_CMD);
    oled_unlock();
}
void OLED_DisplayOff(void)
{
    oled_lock();
    oled_wr_byte(0x8D, OLED_CMD); oled_wr_byte(0x10, OLED_CMD); oled_wr_byte(0xAE, OLED_CMD);
    oled_unlock();
}

/** @brief 清屏：8 页 × 128 列全部写 0 */
void OLED_Clear(void)
{
    uint8_t x, y;
    oled_lock();
    for (y = 0; y < 8; y++)
    {
        OLED_SetPos(0, y);
        for (x = 0; x < 128; x++)
            oled_wr_byte(0x00, OLED_DATA);
    }
    oled_unlock();
}

/* ---------------- 字符显示 ---------------- */

/** @brief 在 (x, y) 处显示一个 ASCII 字符（6x8 字库） */
void OLED_ShowChar(uint8_t x, uint8_t y, char ch)
{
    uint8_t i, c = (uint8_t)ch;
    if (c < 32 || c > 126) c = '?';   /* 超出字库范围显示问号 */
    oled_lock();
    OLED_SetPos(x, y);
    for (i = 0; i < 6; i++)           /* 字库每字符 6 列，逐列写入 */
        oled_wr_byte(F6x8[c - 32][i], OLED_DATA);
    oled_unlock();
}

/** @brief 显示字符串 */
void OLED_ShowString(uint8_t x, uint8_t y, const char *str)
{
    oled_lock();
    while (*str)
    {
        uint8_t i, c = (uint8_t)(*str++);
        if (c < 32 || c > 126) c = '?';
        OLED_SetPos(x, y);
        for (i = 0; i < 6; i++)
            oled_wr_byte(F6x8[c - 32][i], OLED_DATA);
        x += 6;                        /* 每字符宽 6 像素 */
        if (x > 122) break;            /* 超出屏幕宽度截断 */
    }
    oled_unlock();
}

/** @brief 显示无符号整数（len 位，不足补零） */
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len)
{
    char buf[11];
    uint8_t i;
    if (len > 10) len = 10;
    for (i = len; i > 0; i--)          /* 从低位往高位填 */
    {
        buf[i - 1] = (char)('0' + (num % 10));
        num /= 10;
    }
    buf[len] = 0;
    OLED_ShowString(x, y, buf);
}

/** @brief 两位十六进制风格显示：十位与个位各占一个字符位（补零） */
void OLED_ShowTwoNum(uint8_t x, uint8_t y, uint8_t num)
{
    OLED_ShowChar(x,     y, (char)('0' + (num / 10) % 10));
    OLED_ShowChar(x + 6, y, (char)('0' + (num % 10)));
}

/* ================================================================
 *  16 像素行高中文显示（汉字 16x16 / ASCII 8x16）
 *  y 只取 0 / 2 / 4 / 6 —— 每行占 2 个页，整屏刚好 4 行
 * ================================================================ */

/** @brief 取下一个 UTF-8 字符的码点，并把指针往后挪 */
static uint16_t utf8_next(const char **p)
{
    const uint8_t *s = (const uint8_t *)(*p);
    uint16_t cp;

    /* 注意：调用方可能传进来被 strncpy 截断的半截汉字，所以每级都要先确认
     * 后续字节不是字符串结束符，否则会越过 NUL 往后读。 */
    if (s[0] < 0x80)                       /* 0xxxxxxx：单字节 ASCII */
    {
        cp = s[0];
        *p += 1;
    }
    else if ((s[0] & 0xE0) == 0xC0 && s[1] != 0)   /* 110xxxxx：2 字节 */
    {
        cp = (uint16_t)(((s[0] & 0x1F) << 6) | (s[1] & 0x3F));
        *p += 2;
    }
    else if ((s[0] & 0xF0) == 0xE0 && s[1] != 0 && s[2] != 0)  /* 1110xxxx：3 字节（汉字） */
    {
        cp = (uint16_t)(((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F));
        *p += 3;
    }
    else                                   /* 非法/截断字节：当问号处理，只进 1 字节 */
    {
        cp = '?';
        *p += 1;
    }
    return cp;
}

/** @brief 按码点在汉字表里线性查字形，找不到返回 0（画空框） */
static const uint8_t *cn_lookup(uint16_t cp)
{
    uint16_t i;
    for (i = 0; i < CN_FONT_COUNT; i++)
        if (CN_CODE[i] == cp)
            return CN_FONT[i];
    return 0;
}

/** @brief 该串在 16 像素行下的像素宽度（汉字 16、ASCII 8），用于居中 */
uint8_t OLED_TextWidth(const char *utf8)
{
    uint8_t w = 0;
    while (*utf8)
    {
        uint16_t cp = utf8_next(&utf8);
        w = (uint8_t)(w + ((cp < 0x80) ? 8 : 16));
    }
    return w;
}

/** @brief 在 (x, y) 显示一串 UTF-8 文字，可中英混排，行高 16 像素 */
void OLED_ShowText(uint8_t x, uint8_t y, const char *utf8)
{
    oled_lock();
    while (*utf8)
    {
        uint16_t cp = utf8_next(&utf8);
        uint8_t  i;

        if (cp < 0x80)                     /* ---- ASCII：8 宽 ---- */
        {
            uint8_t c = (uint8_t)cp;
            if (c < 32 || c > 126) c = '?';
            if ((uint16_t)x + 8 > 128) break;          /* 出屏截断 */
            OLED_SetPos(x, y);                          /* 上半页 */
            for (i = 0; i < 8; i++)
                oled_wr_byte(F8x16[c - 32][i], OLED_DATA);
            OLED_SetPos(x, (uint8_t)(y + 1));           /* 下半页 */
            for (i = 0; i < 8; i++)
                oled_wr_byte(F8x16[c - 32][8 + i], OLED_DATA);
            x = (uint8_t)(x + 8);
        }
        else                               /* ---- 汉字：16 宽 ---- */
        {
            const uint8_t *g = cn_lookup(cp);
            if ((uint16_t)x + 16 > 128) break;         /* 出屏截断 */
            OLED_SetPos(x, y);
            for (i = 0; i < 16; i++)
                oled_wr_byte(g ? g[i] : (uint8_t)((i == 0 || i == 15) ? 0xFF : 0x42),
                             OLED_DATA);
            OLED_SetPos(x, (uint8_t)(y + 1));
            for (i = 0; i < 16; i++)
                oled_wr_byte(g ? g[16 + i] : (uint8_t)((i == 0 || i == 15) ? 0xFF : 0x42),
                             OLED_DATA);
            x = (uint8_t)(x + 16);
        }
    }
    oled_unlock();
}
