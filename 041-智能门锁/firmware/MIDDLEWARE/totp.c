#include "totp.h"
#include "sha1.h"
#include <string.h>

/**
 * @brief HMAC-SHA1（RFC 2104）
 *        HMAC(K, m) = SHA1( (K⊕opad) || SHA1( (K⊕ipad) || m ) )
 *        密钥超过 64 字节先散列，不足补 0 到 64 字节
 */
static void hmac_sha1(const uint8_t *key, uint32_t key_len,
                      const uint8_t *msg, uint32_t msg_len, uint8_t out[20])
{
    uint8_t k[64];        /* 填充后的密钥       */
    uint8_t kx[64];       /* K⊕ipad / K⊕opad   */
    uint8_t inner[20];    /* 内层散列结果       */
    SHA1_CTX ctx;
    uint8_t  i;

    memset(k, 0, 64);
    if (key_len > 64)
        sha1(k, key, key_len);          /* 长密钥先散列成 20B */
    else
        memcpy(k, key, key_len);

    /* 内层：SHA1( (K⊕0x36…) || m ) */
    for (i = 0; i < 64; i++) kx[i] = k[i] ^ 0x36;
    sha1_init(&ctx);
    sha1_update(&ctx, kx, 64);
    sha1_update(&ctx, msg, msg_len);
    sha1_final(&ctx, inner);

    /* 外层：SHA1( (K⊕0x5c…) || inner ) */
    for (i = 0; i < 64; i++) kx[i] = k[i] ^ 0x5C;
    sha1_init(&ctx);
    sha1_update(&ctx, kx, 64);
    sha1_update(&ctx, inner, 20);
    sha1_final(&ctx, out);
}

/**
 * @brief 计算指定时刻的 6 位 TOTP 口令
 *        动态截断(Dynamic Truncation)：取摘要最后 1 字节的低 4 位作偏移，
 *        从该偏移取 4 字节（最高位清零）对 10^6 取模
 */
uint32_t totp_now(const uint8_t *key, uint32_t key_len, uint32_t unix_time)
{
    uint8_t  mac[20];
    uint8_t  msg[8];
    uint8_t  off;
    uint32_t code;
    uint32_t t = unix_time / TOTP_STEP_SECONDS;
    uint8_t  i;

    /* 时间片转为 8 字节大端 */
    for (i = 0; i < 8; i++)
    {
        msg[7 - i] = (uint8_t)(t & 0xFF);
        t >>= 8;
    }

    hmac_sha1(key, key_len, msg, 8, mac);

    off = mac[19] & 0x0F;               /* 偏移量 0~15 */
    code = ((uint32_t)(mac[off]     & 0x7F) << 24)
         | ((uint32_t) mac[off + 1]        << 16)
         | ((uint32_t) mac[off + 2]        <<  8)
         |  (uint32_t) mac[off + 3];
    return code % 1000000u;             /* 6 位十进制口令 */
}

/**
 * @brief Base32 解码（RFC 4648 字母表 A~Z + 2~7）
 *        Google Authenticator 等令牌 App 的密钥是 Base32 字符串，
 *        解码后才是 HMAC-SHA1 需要的原始密钥字节。
 */
uint8_t base32_decode(const char *in, uint8_t *out, uint8_t out_max)
{
    uint32_t acc = 0;      /* 位累加器   */
    uint8_t  bits = 0;     /* 已积累位数 */
    uint8_t  n = 0;

    for (; *in; in++)
    {
        uint8_t v;
        char c = *in;

        if (c >= 'A' && c <= 'Z')      v = (uint8_t)(c - 'A');
        else if (c >= 'a' && c <= 'z') v = (uint8_t)(c - 'a');
        else if (c >= '2' && c <= '7') v = (uint8_t)(c - '2' + 26);
        else if (c == '=')             break;   /* 填充符结束 */
        else if (c == ' ')             continue;
        else                           return 0; /* 非法字符 */

        acc = (acc << 5) | v;
        bits += 5;
        if (bits >= 8)                 /* 每凑满 8 位输出 1 字节 */
        {
            bits -= 8;
            if (n >= out_max)
                return 0;
            out[n++] = (uint8_t)((acc >> bits) & 0xFF);
        }
    }
    return n;
}

/**
 * @brief 校验用户输入的口令
 *        允许 ±1 个时间窗（DS3231 与手机存在秒级偏差时也能通过）
 */
uint8_t totp_verify(const uint8_t *key, uint32_t key_len,
                    uint32_t unix_time, uint32_t code)
{
    int8_t w;
    for (w = -TOTP_WINDOW; w <= TOTP_WINDOW; w++)
    {
        /* 时间片对齐后直接比较，w 即窗口偏移 */
        if (totp_now(key, key_len,
                     (uint32_t)((int32_t)unix_time + w * TOTP_STEP_SECONDS)) == code)
            return 1;
    }
    return 0;
}
