#include "onenet_token.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "base64.h"
#include "sha1.h"

/*
 * 输出串拼装器：不依赖 stdio / snprintf。
 *
 * 在 STM32 + microlib 环境下，带浮点和完整格式化的 printf 会显著增大
 * Flash，而且这里只需要"追加字符串"和"追加无符号十进制"两件事。
 */
typedef struct {
    char *buffer;
    uint16_t capacity;   /* 含结尾 '\0' 的总容量 */
    uint16_t length;     /* 已写入字符数，不含 '\0' */
    bool overflowed;
} text_builder_t;

static void builder_append_char(text_builder_t *builder, char character)
{
    if ((uint32_t)builder->length + 1U >= (uint32_t)builder->capacity) {
        builder->overflowed = true;
        return;
    }
    builder->buffer[builder->length] = character;
    ++builder->length;
    builder->buffer[builder->length] = '\0';
}

static void builder_append_string(text_builder_t *builder, const char *text)
{
    if (text == NULL) {
        return;
    }
    while (*text != '\0') {
        builder_append_char(builder, *text);
        ++text;
    }
}

static void builder_append_u32(text_builder_t *builder, uint32_t value)
{
    char digits[10];
    uint8_t count = 0U;

    if (value == 0U) {
        builder_append_char(builder, '0');
        return;
    }
    while ((value > 0U) && (count < (uint8_t)sizeof(digits))) {
        digits[count] = (char)('0' + (char)(value % 10U));
        value /= 10U;
        ++count;
    }
    while (count > 0U) {
        --count;
        builder_append_char(builder, digits[count]);
    }
}

/*
 * URL 编码：与参考工程 MIDDLEWARE/onenet_token.c 的行为逐字节一致，
 * 只对 base64 结果里可能出现的 '+', '/', '=' 做百分号转义，
 * 其余字符原样透传。保持一致才能保证生成的 token 与平台既有实现相同。
 */
static uint16_t url_encode(const char *in, char *out, uint16_t out_size)
{
    uint16_t in_index = 0U;
    uint16_t out_index = 0U;

    while ((in[in_index] != '\0') && ((uint16_t)(out_index + 4U) < out_size)) {
        const char character = in[in_index];
        ++in_index;
        if (character == '+') {
            out[out_index++] = '%';
            out[out_index++] = '2';
            out[out_index++] = 'B';
        } else if (character == '/') {
            out[out_index++] = '%';
            out[out_index++] = '2';
            out[out_index++] = 'F';
        } else if (character == '=') {
            out[out_index++] = '%';
            out[out_index++] = '3';
            out[out_index++] = 'D';
        } else {
            out[out_index++] = character;
        }
    }
    out[out_index] = '\0';
    return out_index;
}

uint16_t onenet_calc_token(const onenet_cred_t *cred, uint32_t et,
                           char *out, uint16_t out_size)
{
    uint8_t key_bytes[64];
    uint8_t digest[SHA1_DIGEST_SIZE];
    char sign_base64[BASE64_ENCODE_SIZE(SHA1_DIGEST_SIZE)];
    char sign_encoded[64];
    char resource_raw[96];
    char resource_encoded[128];
    char to_sign[224];
    text_builder_t builder;
    size_t key_length;
    size_t resource_length;

    if ((cred == NULL) || (cred->product_id == NULL) ||
        (cred->device_name == NULL) || (cred->device_secret == NULL) ||
        (out == NULL) || (out_size == 0U)) {
        return 0U;
    }

    /* 1. base64 解码 DeviceSecret 得到 HMAC 密钥 */
    key_length = base64_decode(cred->device_secret,
                               strlen(cred->device_secret),
                               key_bytes);
    if ((key_length == 0U) || (key_length > sizeof(key_bytes))) {
        return 0U;
    }

    /* 2. 拼待签名字符串（按字典序 et → method → res → version，'\n' 分隔） */
    builder = (text_builder_t){to_sign, (uint16_t)sizeof(to_sign), 0U, false};
    builder_append_u32(&builder, et);
    builder_append_string(&builder, "\nsha1\nproducts/");
    builder_append_string(&builder, cred->product_id);
    builder_append_string(&builder, "/devices/");
    builder_append_string(&builder, cred->device_name);
    builder_append_string(&builder, "\n2018-10-31");
    if (builder.overflowed) {
        return 0U;
    }

    /* 3. 签名 sign = base64(HMAC-SHA1(key, to_sign)) */
    hmac_sha1(key_bytes, (size_t)key_length,
              (const uint8_t *)to_sign, (size_t)strlen(to_sign),
              digest);
    if (base64_encode(digest, sizeof(digest), sign_base64) == 0U) {
        return 0U;
    }

    /* 4. res 与 sign 做 URL 编码 */
    resource_length = 0U;
    {
        const char *parts[4] = {"products/", cred->product_id, "/devices/",
                                cred->device_name};
        for (size_t i = 0U; i < 4U; ++i) {
            const size_t part_length = strlen(parts[i]);
            if ((resource_length + part_length) >= sizeof(resource_raw)) {
                return 0U;
            }
            (void)memcpy(&resource_raw[resource_length], parts[i], part_length);
            resource_length += part_length;
        }
        resource_raw[resource_length] = '\0';
    }
    (void)url_encode(resource_raw, resource_encoded, (uint16_t)sizeof(resource_encoded));
    (void)url_encode(sign_base64, sign_encoded, (uint16_t)sizeof(sign_encoded));

    /* 5. 拼完整 token（'&' 本身不需转义） */
    builder = (text_builder_t){out, out_size, 0U, false};
    builder_append_string(&builder, "version=2018-10-31&res=");
    builder_append_string(&builder, resource_encoded);
    builder_append_string(&builder, "&et=");
    builder_append_u32(&builder, et);
    builder_append_string(&builder, "&method=sha1&sign=");
    builder_append_string(&builder, sign_encoded);
    if (builder.overflowed) {
        return 0U;
    }

    /* 6. 擦除栈上的敏感中间量 */
    memset(key_bytes, 0, sizeof(key_bytes));
    memset(digest, 0, sizeof(digest));
    memset(to_sign, 0, sizeof(to_sign));

    return builder.length;
}
