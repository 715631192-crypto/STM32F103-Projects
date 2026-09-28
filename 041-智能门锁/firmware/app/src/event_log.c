#include "event_log.h"

#include <stddef.h>
#include <string.h>

uint32_t event_log_crc32(const void *data, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = UINT32_C(0xFFFFFFFF);
    for (size_t i = 0U; i < length; ++i) {
        crc ^= bytes[i];
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            const uint32_t mask = (uint32_t)(-(int32_t)(crc & 1U));
            crc = (crc >> 1U) ^ (UINT32_C(0xEDB88320) & mask);
        }
    }
    return ~crc;
}

bool event_log_record_valid(const event_log_record_t *record)
{
    uint32_t expected;
    if ((record == NULL) || (record->magic != EVENT_LOG_MAGIC)) {
        return false;
    }
    expected = event_log_crc32(record, offsetof(event_log_record_t, crc32));
    return expected == record->crc32;
}

static bool sequence_is_newer(uint32_t candidate, uint32_t current)
{
    return (int32_t)(candidate - current) > 0;
}

bool event_log_init(event_log_t *log, const event_log_storage_t *storage)
{
    event_log_record_t record;
    bool found = false;
    uint32_t newest_slot = 0U;
    uint32_t newest_sequence = 0U;

    if ((log == NULL) || (storage == NULL) || (storage->slot_count == 0U) ||
        (storage->read == NULL) || (storage->write == NULL) ||
        (storage->erase == NULL)) {
        return false;
    }

    memset(log, 0, sizeof(*log));
    log->storage = *storage;
    for (uint32_t slot = 0U; slot < storage->slot_count; ++slot) {
        if (storage->read(storage->context, slot, &record) &&
            event_log_record_valid(&record)) {
            ++log->valid_count;
            if ((!found) || sequence_is_newer(record.sequence,
                                              newest_sequence)) {
                found = true;
                newest_slot = slot;
                newest_sequence = record.sequence;
            }
        }
    }

    log->next_slot = found ? ((newest_slot + 1U) % storage->slot_count) : 0U;
    log->next_sequence = found ? (newest_sequence + 1U) : 1U;
    log->initialized = true;
    return true;
}

bool event_log_append(event_log_t *log, event_log_record_t *record)
{
    if ((log == NULL) || (record == NULL) || (!log->initialized)) {// 检查参数
        return false;
    }

    record->magic = EVENT_LOG_MAGIC;// 设置魔数
    record->sequence = log->next_sequence;// 设置序列号
    memset(record->reserved, 0, sizeof(record->reserved));// 初始化保留字段
    record->crc32 = event_log_crc32(record,
                                    offsetof(event_log_record_t, crc32));// 计算crc32

    if (!log->storage.erase(log->storage.context, log->next_slot)) {// 擦除旧记录
        return false;
    }
    if (!log->storage.write(log->storage.context, log->next_slot, record)) {// 写入新记录
        return false;
    }

    log->next_slot = (log->next_slot + 1U) % log->storage.slot_count;// 更新下一个槽位
    ++log->next_sequence;// 更新下一个序列号
    if (log->valid_count < log->storage.slot_count) {// 更新有效记录数
        ++log->valid_count;
    }
    return true;
}

bool event_log_read_recent(const event_log_t *log, uint32_t offset,
                           event_log_record_t *record)
{
    uint32_t slot;
    uint32_t expected_sequence;

    if ((log == NULL) || (record == NULL) || (!log->initialized) ||
        (offset >= log->valid_count)) {
        return false;
    }
    slot = (log->next_slot + log->storage.slot_count - 1U - offset) %
           log->storage.slot_count;
    expected_sequence = log->next_sequence - 1U - offset;
    return log->storage.read(log->storage.context, slot, record) &&
           event_log_record_valid(record) &&
           (record->sequence == expected_sequence);
}
