#ifndef __BASE64_H
#define __BASE64_H

#include <stdint.h>

/*
 * Base64 编解码（标准 RFC 4648）
 *   用途：OneNET token = base64(HMAC-SHA1(...))，
 *         TOTP / OAuth 头部等
 *
 *   编码表：ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/
 *   输入 3 字节 → 输出 4 字符；末尾不足 3 字节用 '=' 填充
 */

uint16_t base64_encode(const uint8_t *in, uint16_t in_len, char *out);
uint16_t base64_decode(const char *in, uint16_t in_len, uint8_t *out);

#endif
