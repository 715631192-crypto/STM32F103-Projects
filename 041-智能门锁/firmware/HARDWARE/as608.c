#include "as608.h"
#include "usart.h"
#include "delay.h"

/*
 * 实现说明：
 *  - 发送：按协议组装整包后一次性从 USART2 发出；
 *  - 接收：先找包头 EF 01，再收 长度域，按长度收齐数据+校验和；
 *  - 所有接口阻塞等待应答，超时时间按命令类型区分
 *    （图像采集/搜索较慢，给长超时；其余 1 秒足够）。
 */

/* 模块默认地址 0xFFFFFFFF（未修改过地址的出厂状态） */
static const uint8_t s_addr[4] = { 0xFF, 0xFF, 0xFF, 0xFF };

/* ---------------- 底层收发 ---------------- */

/** @brief 带超时读 1 字节（来自 USART2 环形缓冲） */
static uint8_t rb(uint8_t *b, uint32_t tmo_ms)
{
    uint32_t t0 = millis();
    while ((millis() - t0) < tmo_ms)
    {
        if (usart_read_byte(USART2, b))
            return 1;
    }
    return 0;
}

/** @brief 组包并发送：pid + 数据 → 完整协议帧 */
static void as608_send(uint8_t pid, const uint8_t *data, uint16_t n)
{
    uint8_t  pkt[64];
    uint16_t i, idx = 0, len, sum;

    len = n + 2;                              /* 数据 + 2 字节校验和 */
    pkt[idx++] = 0xEF; pkt[idx++] = 0x01;     /* 包头                */
    pkt[idx++] = s_addr[0]; pkt[idx++] = s_addr[1];
    pkt[idx++] = s_addr[2]; pkt[idx++] = s_addr[3];
    pkt[idx++] = pid;
    pkt[idx++] = (uint8_t)(len >> 8);
    pkt[idx++] = (uint8_t)(len & 0xFF);
    for (i = 0; i < n; i++)
        pkt[idx++] = data[i];
    /* 校验和 = pid + len_hi + len_lo + Σdata */
    sum = pid + (len >> 8) + (len & 0xFF);
    for (i = 0; i < n; i++)
        sum += data[i];
    pkt[idx++] = (uint8_t)(sum >> 8);
    pkt[idx++] = (uint8_t)(sum & 0xFF);

    usart_rx_clear(USART2);                   /* 丢弃残留旧数据 */
    usart_send_bytes(USART2, pkt, idx);
}

/**
 * @brief 接收一个完整应答包
 * @param pkt     输出缓冲（整包原样存放）
 * @param timeout 总超时 ms
 * @return 包总长度；0 = 超时/格式错误
 * 帧内偏移：pkt[6]=pid  pkt[7..8]=长度  pkt[9..]=数据
 */
static uint16_t as608_recv(uint8_t *pkt, uint16_t max, uint32_t timeout)
{
    uint8_t  b;
    uint16_t idx = 0, len, i;
    uint32_t t0 = millis();

    /* 1. 找包头 EF 01 */
    while ((millis() - t0) < timeout)
    {
        if (!rb(&b, timeout - (millis() - t0)))
            return 0;
        if (idx == 0 && b != 0xEF) continue;   /* 未出现 EF 之前的杂波全丢 */
        pkt[idx++] = b;
        if (idx == 1 && b == 0xEF) continue;
        if (idx == 2)                          /* 已收 EF 01 */
        {
            if (b != 0x01) { idx = (b == 0xEF) ? 1 : 0; }
            else break;
        }
    }
    if (idx != 2)
        return 0;

    /* 2. 收地址(4) + 包标识(1) + 长度(2) */
    for (i = 0; i < 7; i++)
    {
        if (!rb(&pkt[idx], timeout))
            return 0;
        idx++;
    }
    len = (uint16_t)((pkt[7] << 8) | pkt[8]);  /* 长度 = 数据 + 校验和2B */
    if (len < 3 || len > 200)
        return 0;                              /* 非法长度 */

    /* 3. 收数据 + 校验和（共 len 字节） */
    for (i = 0; i < len && idx < max; i++)
    {
        if (!rb(&pkt[idx], timeout))
            return 0;
        idx++;
    }
    return idx;
}

/**
 * @brief 发送命令并等待应答（通用封装）
 * @return 确认码(≥0)；AS608_ERR_TIMEOUT=通信失败
 *         ackdata 返回确认码之后的附加数据（如搜索的页码/得分）
 */
static int as608_cmd(const uint8_t *data, uint16_t n, uint32_t timeout,
                     uint8_t *ackdata, uint16_t *acklen)
{
    uint8_t  pkt[64];
    uint16_t got, dlen;

    as608_send(0x01, data, n);
    got = as608_recv(pkt, sizeof(pkt), timeout);
    if (got == 0)
        return AS608_ERR_TIMEOUT;

    dlen = (uint16_t)((pkt[7] << 8) | pkt[8]);  /* 数据域长度(+2 校验) */
    if (dlen < 3)
        return AS608_ERR_TIMEOUT;

    if (ackdata && acklen && dlen > 3)
    {
        uint16_t n2 = dlen - 3;                 /* 确认码之后的附加数据 */
        if (n2 > *acklen) n2 = *acklen;
        for (got = 0; got < n2; got++)
            ackdata[got] = pkt[10 + got];
        *acklen = n2;
    }
    return pkt[9];                              /* 第 1 个数据字节 = 确认码 */
}

/* ---------------- 高级接口 ---------------- */

/** @brief 链路自检：任何应答(包括"无手指"0x02)都证明链路正常 */
int AS608_Check(void)
{
    int r = AS608_GetImage();
    return (r >= 0) ? 1 : 0;
}

/** @brief 采集指纹图像（无手指时返回 0x02，不是错误） */
int AS608_GetImage(void)
{
    uint8_t cmd[1] = { 0x01 };
    return as608_cmd(cmd, 1, 1000, 0, 0);
}

/** @brief 把缓冲区图像生成特征。bufid: 1=第一次 2=第二次 */
int AS608_GenChar(uint8_t bufid)
{
    uint8_t cmd[2];
    cmd[0] = 0x02;
    cmd[1] = bufid;
    return as608_cmd(cmd, 2, 1500, 0, 0);
}

/** @brief 把缓冲区 1、2 的特征合成为模板 */
int AS608_RegModel(void)
{
    uint8_t cmd[1] = { 0x05 };
    return as608_cmd(cmd, 1, 2000, 0, 0);
}

/**
 * @brief 在指纹库范围内搜索比对
 * @return 0=找到（page_id/score 带回结果）  9=未找到  负数=通信失败
 */
int AS608_Search(uint8_t bufid, uint16_t start_page, uint16_t count,
                 uint16_t *page_id, uint16_t *score)
{
    uint8_t  cmd[6];
    uint8_t  ack[8];
    uint16_t alen = sizeof(ack);
    int      r;
    cmd[0] = 0x04;
    cmd[1] = bufid;
    cmd[2] = (uint8_t)(start_page >> 8);
    cmd[3] = (uint8_t)(start_page & 0xFF);
    cmd[4] = (uint8_t)(count >> 8);
    cmd[5] = (uint8_t)(count & 0xFF);
    r = as608_cmd(cmd, 6, 3000, ack, &alen);   /* 搜索耗时较长 */

    /* 应答数据域 = 页码(2B) + 得分(2B)，as608_cmd() 已把"确认码之后"的数据
     * 从 ack[0] 开始铺开，所以：页码 = ack[0]/ack[1]，得分 = ack[2]/ack[3]。
     * ★ 原先写成 ack[1..2] / ack[3..4]，索引整体错位 1 字节：
     *     页码 = (page_lo << 8) | score_hi ≥ 0x0100 = 256，
     *   永远落不进 app_auth_verify_finger() 的 1~99 合法区间
     *   → 指纹识别恒判"失败"，且 3 次就触发 60s 错误锁定。
     *   得分还会读到越界的 ack[4]（alen 只填到 4，该字节从未被赋值）。
     * 另：分级判断长度，兼容"只回页码不回得分"的模块固件。 */
    if (r == 0)
    {
        if (page_id && alen >= 2) *page_id = (uint16_t)((ack[0] << 8) | ack[1]);
        if (score   && alen >= 4) *score   = (uint16_t)((ack[2] << 8) | ack[3]);
    }
    return r;
}

/** @brief 把缓冲区中的模板存入指定页 */
int AS608_StoreChar(uint8_t bufid, uint16_t page_id)
{
    uint8_t cmd[4];
    cmd[0] = 0x06;
    cmd[1] = bufid;
    cmd[2] = (uint8_t)(page_id >> 8);
    cmd[3] = (uint8_t)(page_id & 0xFF);
    return as608_cmd(cmd, 4, 2000, 0, 0);
}

/** @brief 删除指定页开始的 count 个模板 */
int AS608_DeleteChar(uint16_t page_id, uint16_t count)
{
    uint8_t cmd[5];
    cmd[0] = 0x0C;
    cmd[1] = (uint8_t)(page_id >> 8);
    cmd[2] = (uint8_t)(page_id & 0xFF);
    cmd[3] = (uint8_t)(count >> 8);
    cmd[4] = (uint8_t)(count & 0xFF);
    return as608_cmd(cmd, 5, 2000, 0, 0);
}

/** @brief 清空整个指纹库（危险操作，仅恢复出厂用） */
int AS608_Empty(void)
{
    uint8_t cmd[1] = { 0x0D };
    return as608_cmd(cmd, 1, 3000, 0, 0);
}

/**
 * @brief 一键识别：采集→生成特征→搜索
 * @return  0  = 识别成功（page_id/score 带回）
 *          2  = 手指未按下 / 无手指
 *          9  = 库中无匹配
 *         -1  = 模块无应答
 *
 * ★ 搜索分两步（2026-09-11 修正，别改回"一次全库搜"）：
 *   1) 先**只搜胁迫页**（start=DURESS_PAGE, count=1）；
 *   2) 没命中再全库搜 1~USER_PAGE_MAX。
 *
 *   为什么必须先单独搜胁迫页：模块的 PS_Search 只返回**一个**匹配页，
 *   多页命中时通常给页码较小的那个。而实际使用时极容易出现
 *   "胁迫手指同时也是已录入的普通指纹"（同一根手指录了两次），
 *   那样全库一次搜索永远只会返回较小的普通页
 *   ⇒ 胁迫页形同虚设，开锁被记成"普通开锁"、不报警。
 *   先单独搜胁迫页就能让**胁迫身份优先**，与页码大小无关。
 */
int AS608_Identify(uint16_t *page_id, uint16_t *score)
{
    int r = AS608_GetImage();
    if (r < 0)  return AS608_ERR_TIMEOUT;
    if (r != 0) return 2;                       /* 无手指或采集失败 */

    r = AS608_GenChar(1);
    if (r != 0) return (r < 0) ? AS608_ERR_TIMEOUT : 3;

    /* ---- 第一步：只搜胁迫页（命中即认定为胁迫手指） ---- */
    r = AS608_Search(1, AS608_DURESS_PAGE, 1, page_id, score);
    if (r < 0)  return AS608_ERR_TIMEOUT;
    if (r == 0) return 0;                       /* 命中胁迫页 → 立即返回 */

    /* ---- 第二步：普通成员指纹（1 ~ USER_PAGE_MAX，不含胁迫页） ---- */
    r = AS608_Search(1, 1, AS608_USER_PAGE_MAX, page_id, score);
    if (r < 0)  return AS608_ERR_TIMEOUT;
    if (r != 0) return 9;                       /* 未匹配 */
    return 0;
}
