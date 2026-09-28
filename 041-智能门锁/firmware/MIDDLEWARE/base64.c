#include "base64.h"

/* RFC 4648 标准编码表 */
static const char ENC_TBL[64] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * @brief Base64 编码
 * @param in     输入字节
 * @param in_len 输入长度（任意）
 * @param out    输出缓冲；调用方保证足够大（>= (in_len/3+1)*4 + 1）
 * @return       写入字符数（不含 '\0'）
 */
uint16_t base64_encode(const uint8_t *in, uint16_t in_len, char *out)
{
    uint16_t i, j = 0;

    for (i = 0; i + 3 <= in_len; i += 3)
    {
        uint32_t v = ((uint32_t)in[i] << 16)
                   | ((uint32_t)in[i + 1] << 8)
                   | (uint32_t)in[i + 2];
        out[j++] = ENC_TBL[(v >> 18) & 0x3F];
        out[j++] = ENC_TBL[(v >> 12) & 0x3F];
        out[j++] = ENC_TBL[(v >>  6) & 0x3F];
        out[j++] = ENC_TBL[ v        & 0x3F];
    }

    /* 处理尾部 1~2 字节 */
    if (i < in_len)
    {
        uint32_t v = (uint32_t)in[i] << 16;
        if (i + 1 < in_len) v |= (uint32_t)in[i + 1] << 8;

        out[j++] = ENC_TBL[(v >> 18) & 0x3F];
        out[j++] = ENC_TBL[(v >> 12) & 0x3F];
        out[j++] = (i + 1 < in_len) ? ENC_TBL[(v >> 6) & 0x3F] : '=';
        out[j++] = '=';
    }
    out[j] = 0;
    return j;
}

/**
 * @brief Base64 解码（容错：跳过空白，正确处理 '='）
 * @param in     输入字符串
 * @param in_len 输入长度
 * @param out    输出缓冲
 * @return       写入字节数；0 表示输入非法
 */
uint16_t base64_decode(const char *in, uint16_t in_len, uint8_t *out)
{
    uint8_t  DEC[256] = { 0 };   /* 反向表，每次调用重填（C C8T6 RAM 够用） */
    uint16_t i, j = 0;
    uint32_t v = 0;
    uint16_t valid = 0;          /* 已累积的有效 6-bit 字符数 */

    /* 填充反向表：0~63 对应合法字符 */
    for (i = 0; i < 64; i++) DEC[(uint8_t)ENC_TBL[i]] = (uint8_t)i;

    for (i = 0; i < in_len; i++)
    {
        uint8_t c = (uint8_t)in[i];
        if (c == '=') break;                 /* 遇到 padding 立即停止   */
        if (c == '\r' || c == '\n' || c == ' ')
            continue;                        /* 跳过空白                 */

        v = (v << 6) | DEC[c];
        valid++;
        if ((valid & 3) == 0)                /* 收齐 4 个有效字符 */
        {
            out[j++] = (uint8_t)(v >> 16);
            out[j++] = (uint8_t)(v >> 8);
            out[j++] = (uint8_t)v;
            v = 0;
        }
    }

    /* 处理末尾不足 4 字符的剩余数据（无需补 '=' 也能解码）
     *   剩 1 字符 → 6 bit  → 0 字节
     *   剩 2 字符 → 12 bit → 1 字节（高 8 位）
     *   剩 3 字符 → 18 bit → 2 字节 */
    {
        uint16_t rem = valid & 3;
        if (rem == 2)
            out[j++] = (uint8_t)(v >> 4);
        else if (rem == 3)
        {
            out[j++] = (uint8_t)(v >> 10);
            out[j++] = (uint8_t)(v >> 2);
        }
    }
    return j;
}
