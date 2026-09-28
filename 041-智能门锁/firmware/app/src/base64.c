#include "base64.h"

static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* 查表：非 Base64 字符返回 -1，'=' 返回 -2，便于解码时判定结束。 */
static int base64_value(char character)
{
    if ((character >= 'A') && (character <= 'Z')) {
        return (int)(character - 'A');
    }
    if ((character >= 'a') && (character <= 'z')) {
        return (int)(character - 'a') + 26;
    }
    if ((character >= '0') && (character <= '9')) {
        return (int)(character - '0') + 52;
    }
    if (character == '+') {
        return 62;
    }
    if (character == '/') {
        return 63;
    }
    if (character == '=') {
        return -2;
    }
    return -1;
}

size_t base64_encode(const uint8_t *in, size_t in_len, char *out)
{
    size_t written = 0U;
    size_t index = 0U;

    if ((in == NULL) || (out == NULL)) {
        return 0U;
    }

    while ((index + 3U) <= in_len) {
        const uint32_t triple = ((uint32_t)in[index] << 16U) |
                                ((uint32_t)in[index + 1U] << 8U) |
                                (uint32_t)in[index + 2U];
        out[written++] = base64_table[(triple >> 18U) & 0x3FU];
        out[written++] = base64_table[(triple >> 12U) & 0x3FU];
        out[written++] = base64_table[(triple >> 6U) & 0x3FU];
        out[written++] = base64_table[triple & 0x3FU];
        index += 3U;
    }

    if ((in_len - index) == 1U) {
        const uint32_t triple = (uint32_t)in[index] << 16U;
        out[written++] = base64_table[(triple >> 18U) & 0x3FU];
        out[written++] = base64_table[(triple >> 12U) & 0x3FU];
        out[written++] = '=';
        out[written++] = '=';
    } else if ((in_len - index) == 2U) {
        const uint32_t triple = ((uint32_t)in[index] << 16U) |
                                ((uint32_t)in[index + 1U] << 8U);
        out[written++] = base64_table[(triple >> 18U) & 0x3FU];
        out[written++] = base64_table[(triple >> 12U) & 0x3FU];
        out[written++] = base64_table[(triple >> 6U) & 0x3FU];
        out[written++] = '=';
    }

    out[written] = '\0';
    return written;
}

size_t base64_decode(const char *in, size_t in_len, uint8_t *out)
{
    uint32_t accumulator = 0U;
    uint8_t bits = 0U;
    size_t written = 0U;

    if ((in == NULL) || (out == NULL)) {
        return 0U;
    }

    for (size_t i = 0U; i < in_len; ++i) {
        const char character = in[i];
        const int value = base64_value(character);

        if (value == -2) {
            break; /* '='：填充开始，原始数据已取完 */
        }
        if (value < 0) {
            continue; /* 忽略换行、空格、CR 等噪声 */
        }
        accumulator = (accumulator << 6U) | (uint32_t)value;
        bits = (uint8_t)(bits + 6U);
        if (bits >= 8U) {
            bits = (uint8_t)(bits - 8U);
            out[written++] = (uint8_t)((accumulator >> bits) & 0xFFU);
        }
    }
    return written;
}
