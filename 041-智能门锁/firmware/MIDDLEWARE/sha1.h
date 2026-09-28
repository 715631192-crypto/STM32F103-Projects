#ifndef __SHA1_H
#define __SHA1_H

#include <stdint.h>

/*
 * SHA-1 散列算法（FIPS 180-1）
 * ------------------------------------------------
 * 本项目中两处使用：
 *   1. 密码存储：Flash 里只存 SHA-1 摘要，不存明文（金融级要求）；
 *   2. HMAC-SHA1：TOTP 动态口令的核心（见 totp.c）。
 */

typedef struct
{
    uint32_t state[5];     /* A~E 五个状态字            */
    uint32_t count;        /* 已处理字节数               */
    uint8_t  buffer[64];   /* 不足一块的暂存区           */
} SHA1_CTX;

void sha1(uint8_t out[20], const uint8_t *data, uint32_t len);  /* 一步式接口 */

void sha1_init(SHA1_CTX *ctx);
void sha1_update(SHA1_CTX *ctx, const uint8_t *data, uint32_t len);
void sha1_final(SHA1_CTX *ctx, uint8_t out[20]);

/**
 * @brief HMAC-SHA1：RFC 2104 标准
 *        用途：OneNET Studio MQTT token 鉴权、TOTP
 * @param key     密钥（任意长度，<=64 字节内部填充，否则先 SHA-1）
 * @param key_len 密钥字节数
 * @param msg     要签名的数据
 * @param msg_len 数据字节数
 * @param out     20 字节输出
 */
void hmac_sha1(const uint8_t *key, uint32_t key_len,
               const uint8_t *msg, uint32_t msg_len,
               uint8_t out[20]);

#endif
