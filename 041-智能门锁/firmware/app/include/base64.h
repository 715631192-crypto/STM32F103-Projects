#ifndef SMART_LOCK_BASE64_H
#define SMART_LOCK_BASE64_H

#include <stddef.h>
#include <stdint.h>

/*
 * Base64（RFC 4648 标准表）。
 *
 * 本工程自带一份实现，而不是复用参考工程的 MIDDLEWARE/base64.c，
 * 目的是让 OneNET token 的计算可以在主机上完整跑单元测试。
 *
 * 编码表：A-Z a-z 0-9 + /
 * 每 3 字节输入 → 4 字符输出，不足用 '=' 补齐。
 */

/* 编码结果长度（含 '\0'）：4 * ceil(in_len/3) + 1 */
#define BASE64_ENCODE_SIZE(in_len) ((((in_len) + 2U) / 3U) * 4U + 1U)

/* 解码结果长度上限：3 * ceil(in_len/4) */
#define BASE64_DECODE_MAX(in_len) ((((in_len) + 3U) / 4U) * 3U)

/* 成功返回写入的字符数（不含 '\0'），失败返回 0。
 * out 至少需要 BASE64_ENCODE_SIZE(in_len) 字节。 */
size_t base64_encode(const uint8_t *in, size_t in_len, char *out);

/* 成功返回解码出的字节数，失败返回 0。忽略换行，'=' 作为结束标记。 */
size_t base64_decode(const char *in, size_t in_len, uint8_t *out);

#endif /* SMART_LOCK_BASE64_H */
