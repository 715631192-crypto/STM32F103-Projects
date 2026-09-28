#include "onenet_token.h"
#include "sha1.h"
#include "base64.h"
#include <string.h>
#include <stdio.h>

/**
 * @brief URL-encode（仅处理 + / = 与非字母数字/-_.~ 的字符）
 *        OneNET token 各字段需要 URL 编码后再拼接
 */
static uint16_t url_encode(const char *in, char *out, uint16_t out_size)
{
    uint16_t i = 0, j = 0;
    while (in[i] && j + 4 < out_size)
    {
        char c = in[i++];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
            || (c >= '0' && c <= '9')
            || c == '-' || c == '_' || c == '.' || c == '~')
        {
            out[j++] = c;
        }
        else if (c == '+')      { out[j++] = '%'; out[j++] = '2'; out[j++] = 'B'; }
        else if (c == '/')      { out[j++] = '%'; out[j++] = '2'; out[j++] = 'F'; }
        else if (c == '=')      { out[j++] = '%'; out[j++] = '3'; out[j++] = 'D'; }
        else                    { out[j++] = c; }
    }
    out[j] = 0;
    return j;
}

/**
 * @brief 计算 OneNET 设备 token（完整 string 形态）
 *
 * 官方步骤：
 *   1. base64_decode(DeviceSecret) → 32 字节密钥
 *   2. 拼 StringForSignature：
 *      "{et}\nsha1\nproducts/{pid}/devices/{dn}\n2018-10-31"
 *      （按字典序 et → method → res → version，'\n' 分隔）
 *   3. sign = base64(HMAC-SHA1(key, StringForSignature))
 *   4. URL-encode sign
 *   5. URL-encode res = "products/{pid}/devices/{dn}"
 *   6. 拼接完整 token string
 */
uint16_t onenet_calc_token(const onenet_cred_t *cred, uint32_t et,
                           char *out, uint16_t out_size)
{
    char     msg[256];
    char     res_raw[96];
    char     res_enc[128];
    char     sign_raw[40];
    char     sign_enc[60];
    uint8_t  key_bytes[64];
    uint8_t  hmac_out[20];
    uint16_t klen;

    /* 1. base64 解码密钥 */
    klen = base64_decode(cred->device_secret,
                         (uint16_t)strlen(cred->device_secret),
                         key_bytes);

    /* 2. 拼 StringForSignature（4 段，'\n' 分隔） */
    snprintf(msg, sizeof(msg),
             "%lu\nsha1\nproducts/%s/devices/%s\n2018-10-31",
             (unsigned long)et, cred->product_id, cred->device_name);

    /* 3. HMAC-SHA1 */
    hmac_sha1(key_bytes, klen,
              (const uint8_t *)msg, (uint32_t)strlen(msg),
              hmac_out);

    /* 4. base64 编码签名 */
    base64_encode(hmac_out, 20, sign_raw);

    /* 5. URL-encode sign 和 res */
    url_encode(sign_raw, sign_enc, sizeof(sign_enc));
    snprintf(res_raw, sizeof(res_raw),
             "products/%s/devices/%s",
             cred->product_id, cred->device_name);
    url_encode(res_raw, res_enc, sizeof(res_enc));

    /* 6. 拼完整 token（注意：& 不需要 URL 编码） */
    snprintf(out, out_size,
             "version=2018-10-31"
             "&res=%s"
             "&et=%lu"
             "&method=sha1"
             "&sign=%s",
             res_enc, (unsigned long)et, sign_enc);

    /* 清零敏感缓冲 */
    memset(key_bytes, 0, sizeof(key_bytes));
    memset(hmac_out, 0, sizeof(hmac_out));

    return (uint16_t)strlen(out);
}
