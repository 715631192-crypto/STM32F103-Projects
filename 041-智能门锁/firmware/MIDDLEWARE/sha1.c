#include "sha1.h"

/*
 * SHA-1 标准实现：分组 512bit(64B)，输出 160bit(20B) 摘要。
 * 80 轮运算分 4 段，每段 20 轮，使用不同的轮函数与常数。
 */

/** @brief 32 位循环左移 */
static uint32_t rol(uint32_t v, uint8_t b)
{
    return (v << b) | (v >> (32 - b));
}

/** @brief 处理一个 512bit 分块，更新状态字 */
static void sha1_block(SHA1_CTX *ctx, const uint8_t p[64])
{
    uint32_t w[80];
    uint32_t a, b, c, d, e, t;
    uint8_t  i;

    /* 1. 分块扩展：前 16 字大端装载，后 64 字由异或+循环左移生成 */
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16)
             | ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    for (i = 16; i < 80; i++)
        w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];

    /* 2. 80 轮主循环 */
    for (i = 0; i < 80; i++)
    {
        if (i < 20)
            t = rol(a, 5) + ((b & c) | ((~b) & d)) + 0x5A827999u + e + w[i];
        else if (i < 40)
            t = rol(a, 5) + (b ^ c ^ d) + 0x6ED9EBA1u + e + w[i];
        else if (i < 60)
            t = rol(a, 5) + ((b & c) | (b & d) | (c & d)) + 0x8F1BBCDCu + e + w[i];
        else
            t = rol(a, 5) + (b ^ c ^ d) + 0xCA62C1D6u + e + w[i];
        e = d;
        d = c;
        c = rol(b, 30);
        b = a;
        a = t;
    }

    /* 3. 累加回状态 */
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
}

void sha1_init(SHA1_CTX *ctx)
{
    ctx->count = 0;
    ctx->state[0] = 0x67452301u;   /* 标准初始向量 */
    ctx->state[1] = 0xEFCDAB89u;
    ctx->state[2] = 0x98BADCFEu;
    ctx->state[3] = 0x10325476u;
    ctx->state[4] = 0xC3D2E1F0u;
}

void sha1_update(SHA1_CTX *ctx, const uint8_t *data, uint32_t len)
{
    uint32_t i = 0, j;

    j = ctx->count % 64;                 /* 暂存区已有字节数 */
    ctx->count += len;

    if (j)                               /* 先把暂存区凑满一块 */
    {
        for (; i < len && j < 64; i++, j++)
            ctx->buffer[j] = data[i];
        if (j == 64)
            sha1_block(ctx, ctx->buffer);
    }

    for (; (i + 64) <= len; i += 64)     /* 整块直接处理 */
        sha1_block(ctx, data + i);

    for (j = 0; i < len; i++, j++)       /* 余数进暂存区 */
        ctx->buffer[j] = data[i];
}

void sha1_final(SHA1_CTX *ctx, uint8_t out[20])
{
    uint32_t total = ctx->count;         /* 保存原始长度再填充 */
    uint8_t  pad[72];
    uint8_t  len8[8];
    uint8_t  i, padlen;

    /* 8 字节大端比特长度（本实现消息 < 512MB，高 32 位为 0） */
    len8[0] = len8[1] = len8[2] = len8[3] = 0;
    len8[4] = (uint8_t)(total >> 21);            /* total*8 的 bit24~31 */
    len8[5] = (uint8_t)((total >> 13) & 0xFF);
    len8[6] = (uint8_t)((total >> 5) & 0xFF);
    len8[7] = (uint8_t)((total << 3) & 0xFF);

    /* 填充规则：0x80 + 若干 0x00，使总长 ≡ 56 (mod 64) */
    padlen = (ctx->count % 64 < 56) ? (uint8_t)(56 - ctx->count % 64)
                                    : (uint8_t)(120 - ctx->count % 64);
    pad[0] = 0x80;
    for (i = 1; i < padlen; i++)
        pad[i] = 0;

    sha1_update(ctx, pad, padlen);       /* 填充字节 */
    sha1_update(ctx, len8, 8);           /* 长度字节 */

    /* 状态字按大端输出 20 字节摘要 */
    for (i = 0; i < 5; i++)
    {
        out[i * 4]     = (uint8_t)(ctx->state[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        out[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

/** @brief 一步式：对一段数据直接计算 SHA-1 */
void sha1(uint8_t out[20], const uint8_t *data, uint32_t len)
{
    SHA1_CTX ctx;
    sha1_init(&ctx);
    sha1_update(&ctx, data, len);
    sha1_final(&ctx, out);
}

/*
 * HMAC-SHA1 (RFC 2104)
 *   opad = 0x5c 重复 64 次
 *   ipad = 0x36 重复 64 次
 *   HMAC = SHA1((key XOR opad) || SHA1((key XOR ipad) || msg))
 *   当 key 长度 > 64 时，先对 key 做 SHA-1 再填充 0
 */
void hmac_sha1(const uint8_t *key, uint32_t key_len,
               const uint8_t *msg, uint32_t msg_len,
               uint8_t out[20])
{
    uint8_t  k_pad[64];          /* key XOR (ipad/opad)        */
    uint8_t  k_norm[64];         /* 规整化的 key（长度归一到64） */
    SHA1_CTX ctx;
    uint8_t  inner[20];          /* 内层 SHA-1 结果            */
    uint8_t  i;

    /* 1. 规整化 key：>64 先哈希，否则补 0 到 64 */
    if (key_len > 64)
    {
        sha1(k_norm, key, key_len);
        for (i = 20; i < 64; i++) k_norm[i] = 0;
    }
    else
    {
        for (i = 0; i < key_len; i++) k_norm[i] = key[i];
        for (; i < 64; i++)         k_norm[i] = 0;
    }

    /* 2. 内层：SHA1((k XOR ipad) || msg) */
    for (i = 0; i < 64; i++) k_pad[i] = k_norm[i] ^ 0x36;
    sha1_init(&ctx);
    sha1_update(&ctx, k_pad, 64);
    sha1_update(&ctx, msg, msg_len);
    sha1_final(&ctx, inner);

    /* 3. 外层：SHA1((k XOR opad) || inner) */
    for (i = 0; i < 64; i++) k_pad[i] = k_norm[i] ^ 0x5c;
    sha1_init(&ctx);
    sha1_update(&ctx, k_pad, 64);
    sha1_update(&ctx, inner, 20);
    sha1_final(&ctx, out);
}
