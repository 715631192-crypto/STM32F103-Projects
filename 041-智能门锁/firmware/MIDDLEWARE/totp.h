#ifndef __TOTP_H
#define __TOTP_H

#include <stdint.h>

/*
 * TOTP 离线动态口令（RFC 6238，基于 HMAC-SHA1）
 * ------------------------------------------------
 * 原理：与"银行令牌"相同——
 *   code = Truncate( HMAC-SHA1(密钥, 当前时间片) ) mod 1000000
 *   时间片 = Unix 时间戳 / 30（每 30 秒换一个 6 位口令）
 *
 * 门锁端用 DS3231 的可信时间 + 预共享密钥本地计算，
 * 手机端（任意 TOTP App，如 Google Authenticator）用同一密钥
 * 生成相同口令 → 无需联网即可完成一次性开锁码验证。
 */

#define TOTP_STEP_SECONDS  30      /* 口令有效窗口：30 秒 */
#define TOTP_WINDOW        1       /* 校验容差：前后各 1 个窗口(共90s) */

uint32_t totp_now(const uint8_t *key, uint32_t key_len, uint32_t unix_time);
/* 生成当前 6 位口令 */

uint8_t  totp_verify(const uint8_t *key, uint32_t key_len,
                     uint32_t unix_time, uint32_t code);
/* 校验输入口令，1=通过 */

uint8_t  base32_decode(const char *in, uint8_t *out, uint8_t out_max);
/* Base32 解码（RFC 4648），用于把手机令牌 App 的密钥字符串
 * 转成 HMAC 原始字节。返回解码字节数，出错返回 0 */

#endif
