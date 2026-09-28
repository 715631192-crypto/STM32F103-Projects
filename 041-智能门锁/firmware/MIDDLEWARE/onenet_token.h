#ifndef __ONENET_TOKEN_H
#define __ONENET_TOKEN_H

#include <stdint.h>

/*
 * OneNET Studio MQTT 设备 token 计算（官方版）
 * ================================================================
 * 依据：https://open.iot.10086.cn/doc/aiot/fuse/detail/1486
 *
 * token 形态：完整 query string，要 URL-encode 后作 MQTT password：
 *   version=2018-10-31
 *   &res=<URL编码后的 products/{pid}/devices/{dn}>
 *   &et=<Unix 时间戳>
 *   &method=sha1
 *   &sign=<URL编码后的 base64(HMAC-SHA1)>
 *
 * StringForSignature = "{et}\nsha1\nproducts/{pid}/devices/{dn}\n2018-10-31"
 *                      ↑ 参数按字典序 et→method→res→version，'\n' 分隔
 *
 * sign = base64( HMAC-SHA1( base64decode(DeviceSecret), StringForSignature ) )
 *
 * 验证：用 OneNET 官方文档示例（pid=238322, key=EnW4T71eIE55...）算出的 token
 *       与文档中给出的 password 完全一致。
 */

typedef struct {
    const char *product_id;     /* 10 位 ProductID */
    const char *device_name;    /* DeviceName */
    const char *device_secret;  /* base64 编码后的 DeviceSecret（44 字符） */
} onenet_cred_t;

/**
 * @brief 计算 OneNET 设备 token（完整 string，含 5 个 URL-encoded 参数）
 * @param cred      三元组
 * @param et        过期时间戳（秒，建议当前时间 + 365 天）
 * @param out       输出缓冲（建议 ≥ 200 字节）
 * @param out_size  缓冲大小
 * @return          实际写入字符数（不含 '\0'）；0 表示出错
 */
uint16_t onenet_calc_token(const onenet_cred_t *cred, uint32_t et,
                           char *out, uint16_t out_size);

#endif
