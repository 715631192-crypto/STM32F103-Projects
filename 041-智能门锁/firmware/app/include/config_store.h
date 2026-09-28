#ifndef SMART_LOCK_CONFIG_STORE_H
#define SMART_LOCK_CONFIG_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "board_port.h"

/*
 * 安全配置的持久化：带版本号 + 序号的 CRC32 信封，A/B 双槽掉电安全写入。
 *
 * 为什么需要它
 * ------------
 * board_port.h 要求 board_security_load/save 使用"带版本、带 CRC、
 * 掉电安全"的信封，但仓库里此前没有任何实现。本模块把它做成纯逻辑 +
 * 可注入的 Flash 读写回调，因此可以在主机上完整测试"写一半掉电"
 * 这类场景，而不用真的去拔电源。
 *
 * 槽位切换规则
 * ------------
 *   - 读：两个槽都解析，取"CRC 正确且序号较大"的那个。
 *   - 写：永远写"当前非活动"的那个槽（先擦后写），序号 = 活动序号 + 1。
 *     这样在擦/写过程中断电，旧槽仍然完好，配置不会丢。
 *
 * 信封格式（小端）
 * ------------
 *   offset 0  : 魔数 magic   'S''L''C''1'  = 0x31434C53
 *   offset 4  : 版本号 version u16
 *   offset 6  : 有效载荷长度 payload_length u16
 *   offset 8  : 序号 sequence u32
 *   offset 12 : 有效载荷 payload[payload_length]
 *   末尾 4 字节: crc32（覆盖 magic..payload 的全部字节）
 *
 * payload 内部逐字段编码，不直接 memcpy 结构体，避免编译器填充字节
 * 带来的跨平台差异。
 */

#define CONFIG_STORE_MAGIC        UINT32_C(0x31434C53)
#define CONFIG_STORE_VERSION      UINT16_C(1)
#define CONFIG_STORE_HEADER_SIZE  12U
#define CONFIG_STORE_CRC_SIZE     4U
#define CONFIG_STORE_SLOT_SIZE    4096U

/* payload 编码后的固定长度（字段固定，长度恒定） */
#define CONFIG_STORE_PAYLOAD_SIZE (BOARD_DEVICE_SECRET_SIZE + \
                                   BOARD_TOTP_SECRET_MAX_SIZE + 1U + \
                                   ((SHA1_DIGEST_SIZE + 2U) * 2U) + \
                                   ((SHA1_DIGEST_SIZE + 2U) + 1U + 2U + 2U + 2U + 1U) + \
                                   (SHA1_DIGEST_SIZE + 8U + 8U + 1U + 1U) + \
                                   8U + 1U)

/* 一个槽能装下的最大信封长度 */
#define CONFIG_STORE_ENVELOPE_MAX_SIZE 256U

typedef bool (*config_store_read_fn)(void *context, uint32_t offset,
                                     uint8_t *data, uint32_t length);
typedef bool (*config_store_write_fn)(void *context, uint32_t offset,
                                      const uint8_t *data, uint32_t length);
typedef bool (*config_store_erase_fn)(void *context, uint32_t address);

typedef struct {
    void *context;
    uint32_t slot_a_address;
    uint32_t slot_b_address;
    config_store_read_fn read;
    config_store_write_fn write;
    config_store_erase_fn erase;
} config_store_media_t;

/* 把配置编码进信封。成功返回信封总字节数，失败返回 0。 */
uint32_t config_store_serialize(const board_security_config_t *config,
                                uint8_t *out, uint32_t out_size);

/* 解析信封。magic / version / 长度 / CRC 任一不符即返回 false。 */
bool config_store_deserialize(const uint8_t *in, uint32_t in_size,
                              board_security_config_t *config);

/* 从两个槽里读回最新的合法配置。两槽都不可用时返回 false。 */
bool config_store_load(const config_store_media_t *media,
                       board_security_config_t *config);

/* 写入当前非活动槽。返回 true 表示新配置已落盘且可读回。 */
bool config_store_save(const config_store_media_t *media,
                       const board_security_config_t *config);

#endif /* SMART_LOCK_CONFIG_STORE_H */
