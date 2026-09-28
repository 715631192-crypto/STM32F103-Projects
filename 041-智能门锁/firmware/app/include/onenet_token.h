#ifndef SMART_LOCK_ONENET_TOKEN_H
#define SMART_LOCK_ONENET_TOKEN_H

#include <stdint.h>

/*
 * OneNET Studio 设备 token 计算（MQTT 密码）。
 *
 * 本文件刻意与参考工程 MIDDLEWARE/onenet_token.h **保持同名同接口**，
 * 这样参考工程的 HARDWARE/esp8266.c 无需任何改动即可直接编译链接：
 *     onenet_cred_t      —— 三元组
 *     onenet_calc_token  —— 生成密码串
 *
 * 但实现改为调用本工程自带的 hmac_sha1（firmware/app/src/sha1.c），
 * 因此整个 token 计算可以在主机上跑单测，且全工程只有一份 SHA-1。
 *
 * 算法（OneNET 官方 2018-10-31 版）：
 *   1. 密钥 key      = base64_decode(DeviceSecret)
 *   2. to_sign  = "{et}\nsha1\nproducts/{pid}/devices/{dn}\n2018-10-31"
 *   3. 签名 sign     = base64(HMAC-SHA1(key, to_sign))
 *   4. 口令 password = "version=2018-10-31&res=<urlenc(res)>&et=<et>"
 *                 "&method=sha1&sign=<urlenc(sign)>"
 */

typedef struct {
    const char *product_id;     /* ProductID，例如 "v4nyue7h41" */
    const char *device_name;    /* DeviceName，例如 "smartlock_001" */
    const char *device_secret;  /* base64 编码的 DeviceSecret */
} onenet_cred_t;

/*
 * 生成完整 token query string。
 * et        —— 过期 Unix 时间戳（秒），建议 当前时间 + 365 天
 * out       —— 输出缓冲，建议 >= 256 字节
 * out_size  —— 输出缓冲字节数
 * 返回写入的字符数（不含 '\0'）；0 表示参数非法或缓冲不足。
 */
uint16_t onenet_calc_token(const onenet_cred_t *cred, uint32_t et,
                           char *out, uint16_t out_size);

#endif /* SMART_LOCK_ONENET_TOKEN_H */
