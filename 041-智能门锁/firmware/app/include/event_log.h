#ifndef SMART_LOCK_EVENT_LOG_H
#define SMART_LOCK_EVENT_LOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EVENT_LOG_MAGIC UINT32_C(0x534C4F47)

typedef enum {
    LOCK_EVENT_BOOT = 1,
    LOCK_EVENT_UNLOCK,
    LOCK_EVENT_LOCK,
    LOCK_EVENT_AUTH_FAILURE,
    LOCK_EVENT_LOCKOUT,
    LOCK_EVENT_TAMPER,
    LOCK_EVENT_DOOR_AJAR,
    LOCK_EVENT_NETWORK,
    LOCK_EVENT_DURESS,
    LOCK_EVENT_STORAGE_FAILURE,
    LOCK_EVENT_QUEUE_OVERFLOW,
    /*
     * 设备端改主密码（键盘 'D' 向导）成功落盘。
     *
     * ★ 必须追加在枚举**末尾**：event_type 在 W25Q64 上按 uint8_t 持久化，
     * 插在中间会把历史记录的取值整体挪位（旧的 QUEUE_OVERFLOW 被读成
     * CONFIG_CHANGE）。追加则所有旧取值保持不变。
     */
    LOCK_EVENT_CONFIG_CHANGE,
    /*
     * 2026-09-14 管理菜单新增：登记（录指纹/录卡/加密码）与删除。
     * 同样只能追加在末尾，理由同上。
     */
    LOCK_EVENT_ENROLL,
    LOCK_EVENT_DELETE
} lock_event_type_t;

typedef enum {
    LOCK_METHOD_SYSTEM = 0,
    LOCK_METHOD_PIN,
    LOCK_METHOD_FINGERPRINT,
    LOCK_METHOD_RFID,
    LOCK_METHOD_TOTP,
    LOCK_METHOD_VISITOR,
    LOCK_METHOD_REMOTE,
    LOCK_METHOD_PERIODIC
} lock_auth_method_t;

typedef struct {
    uint32_t magic;
    uint32_t sequence;
    uint64_t unix_time;
    uint16_t user_id;
    uint8_t event_type;
    uint8_t auth_method;
    uint8_t result;
    uint8_t reserved[3];
    uint32_t crc32;
} event_log_record_t;

typedef bool (*event_log_read_fn)(void *context, uint32_t slot,
                                  event_log_record_t *record);
typedef bool (*event_log_write_fn)(void *context, uint32_t slot,
                                   const event_log_record_t *record);
typedef bool (*event_log_erase_fn)(void *context, uint32_t slot);

typedef struct {
    void *context;
    uint32_t slot_count;
    event_log_read_fn read;
    event_log_write_fn write;
    event_log_erase_fn erase;
} event_log_storage_t;

typedef struct {
    event_log_storage_t storage;
    uint32_t next_slot;
    uint32_t next_sequence;
    uint32_t valid_count;
    bool initialized;
} event_log_t;

uint32_t event_log_crc32(const void *data, size_t length);
bool event_log_record_valid(const event_log_record_t *record);
bool event_log_init(event_log_t *log, const event_log_storage_t *storage);
bool event_log_append(event_log_t *log, event_log_record_t *record);
bool event_log_read_recent(const event_log_t *log, uint32_t offset,
                           event_log_record_t *record);

#endif
