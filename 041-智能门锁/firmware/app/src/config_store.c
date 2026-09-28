#include "config_store.h"

#include <string.h>

#include "event_log.h"

/* ------------------------------------------------------------------ */
/* 小端字节序读写游标                                                  */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t *data;
    uint32_t capacity;
    uint32_t offset;
    bool overflowed;
} writer_t;

typedef struct {
    const uint8_t *data;
    uint32_t length;
    uint32_t offset;
    bool overflowed;
} reader_t;

static void put_u8(writer_t *writer, uint8_t value)
{
    if ((writer->offset + 1U) > writer->capacity) {
        writer->overflowed = true;
        return;
    }
    writer->data[writer->offset] = value;
    writer->offset += 1U;
}

static void put_u16(writer_t *writer, uint16_t value)
{
    put_u8(writer, (uint8_t)(value & 0xFFU));
    put_u8(writer, (uint8_t)((value >> 8U) & 0xFFU));
}

static void put_u32(writer_t *writer, uint32_t value)
{
    put_u8(writer, (uint8_t)(value & 0xFFU));
    put_u8(writer, (uint8_t)((value >> 8U) & 0xFFU));
    put_u8(writer, (uint8_t)((value >> 16U) & 0xFFU));
    put_u8(writer, (uint8_t)((value >> 24U) & 0xFFU));
}

static void put_u64(writer_t *writer, uint64_t value)
{
    put_u32(writer, (uint32_t)(value & UINT32_MAX));
    put_u32(writer, (uint32_t)((value >> 32U) & UINT32_MAX));
}

static void put_bytes(writer_t *writer, const uint8_t *data, uint32_t length)
{
    if ((writer->offset + length) > writer->capacity) {
        writer->overflowed = true;
        return;
    }
    (void)memcpy(&writer->data[writer->offset], data, length);
    writer->offset += length;
}

static uint8_t get_u8(reader_t *reader)
{
    if ((reader->offset + 1U) > reader->length) {
        reader->overflowed = true;
        return 0U;
    }
    {
        const uint8_t value = reader->data[reader->offset];
        reader->offset += 1U;
        return value;
    }
}

static uint16_t get_u16(reader_t *reader)
{
    const uint16_t low = (uint16_t)get_u8(reader);
    const uint16_t high = (uint16_t)get_u8(reader);
    return (uint16_t)(low | (uint16_t)(high << 8U));
}

static uint32_t get_u32(reader_t *reader)
{
    const uint32_t byte0 = (uint32_t)get_u8(reader);
    const uint32_t byte1 = (uint32_t)get_u8(reader);
    const uint32_t byte2 = (uint32_t)get_u8(reader);
    const uint32_t byte3 = (uint32_t)get_u8(reader);
    return byte0 | (byte1 << 8U) | (byte2 << 16U) | (byte3 << 24U);
}

static uint64_t get_u64(reader_t *reader)
{
    const uint64_t low = (uint64_t)get_u32(reader);
    const uint64_t high = (uint64_t)get_u32(reader);
    return low | (high << 32U);
}

static void get_bytes(reader_t *reader, uint8_t *out, uint32_t length)
{
    if ((reader->offset + length) > reader->length) {
        reader->overflowed = true;
        return;
    }
    (void)memcpy(out, &reader->data[reader->offset], length);
    reader->offset += length;
}

/* ------------------------------------------------------------------ */
/* 字段编解码                                                          */
/* ------------------------------------------------------------------ */

static void put_pin_credential(writer_t *writer, const pin_credential_t *pin)
{
    put_bytes(writer, pin->tag, SHA1_DIGEST_SIZE);
    put_u8(writer, pin->pin_length);
    put_u8(writer, pin->valid ? 1U : 0U);
}

static bool get_pin_credential(reader_t *reader, pin_credential_t *pin)
{
    get_bytes(reader, pin->tag, SHA1_DIGEST_SIZE);
    pin->pin_length = get_u8(reader);
    pin->valid = (get_u8(reader) != 0U) && (!reader->overflowed);
    return !reader->overflowed;
}

static void config_encode(const board_security_config_t *config,
                          writer_t *writer)
{
    put_bytes(writer, config->device_secret, BOARD_DEVICE_SECRET_SIZE);
    put_bytes(writer, config->totp_secret, BOARD_TOTP_SECRET_MAX_SIZE);
    put_u8(writer, config->totp_secret_length);

    put_pin_credential(writer, &config->owner_pin);
    put_pin_credential(writer, &config->duress_pin);
    put_pin_credential(writer, &config->periodic_pin.pin);

    put_u8(writer, config->periodic_pin.days_mask);
    put_u16(writer, config->periodic_pin.start_minute);
    put_u16(writer, config->periodic_pin.end_minute);
    put_u16(writer, (uint16_t)config->periodic_pin.utc_offset_minutes);
    put_u8(writer, config->periodic_pin.enabled ? 1U : 0U);

    put_bytes(writer, config->visitor.code_tag, SHA1_DIGEST_SIZE);
    put_u64(writer, config->visitor.valid_from);
    put_u64(writer, config->visitor.valid_until);
    put_u8(writer, config->visitor.uses_remaining);
    put_u8(writer, config->visitor.active ? 1U : 0U);

    put_u64(writer, config->last_remote_request_id);
    put_u8(writer, (uint8_t)config->second_factor_mode);
}

static bool config_decode(reader_t *reader, board_security_config_t *config)
{
    memset(config, 0, sizeof(*config));

    get_bytes(reader, config->device_secret, BOARD_DEVICE_SECRET_SIZE);
    get_bytes(reader, config->totp_secret, BOARD_TOTP_SECRET_MAX_SIZE);
    config->totp_secret_length = get_u8(reader);

    if (!get_pin_credential(reader, &config->owner_pin) ||
        !get_pin_credential(reader, &config->duress_pin) ||
        !get_pin_credential(reader, &config->periodic_pin.pin)) {
        return false;
    }

    config->periodic_pin.days_mask = get_u8(reader);
    config->periodic_pin.start_minute = get_u16(reader);
    config->periodic_pin.end_minute = get_u16(reader);
    config->periodic_pin.utc_offset_minutes = (int16_t)get_u16(reader);
    config->periodic_pin.enabled = (get_u8(reader) != 0U);

    get_bytes(reader, config->visitor.code_tag, SHA1_DIGEST_SIZE);
    config->visitor.valid_from = get_u64(reader);
    config->visitor.valid_until = get_u64(reader);
    config->visitor.uses_remaining = get_u8(reader);
    config->visitor.active = (get_u8(reader) != 0U);

    config->last_remote_request_id = get_u64(reader);
    config->second_factor_mode = (board_second_factor_mode_t)get_u8(reader);

    return !reader->overflowed;
}

/* ------------------------------------------------------------------ */
/* 信封读写                                                            */
/* ------------------------------------------------------------------ */

uint32_t config_store_serialize(const board_security_config_t *config,
                                uint8_t *out, uint32_t out_size)
{
    writer_t writer;
    uint32_t total;
    uint32_t crc;

    if ((config == NULL) || (out == NULL) ||
        (out_size < CONFIG_STORE_ENVELOPE_MAX_SIZE)) {
        return 0U;
    }

    writer = (writer_t){out, out_size, 0U, false};
    put_u32(&writer, CONFIG_STORE_MAGIC);
    put_u16(&writer, CONFIG_STORE_VERSION);
    put_u16(&writer, (uint16_t)CONFIG_STORE_PAYLOAD_SIZE);
    put_u32(&writer, 0U); /* sequence 占位，由调用方在写槽时回填 */

    config_encode(config, &writer);
    if (writer.overflowed || (writer.offset !=
                              (CONFIG_STORE_HEADER_SIZE + CONFIG_STORE_PAYLOAD_SIZE))) {
        return 0U;
    }

    total = writer.offset + CONFIG_STORE_CRC_SIZE;
    crc = event_log_crc32(out, writer.offset);
    put_u32(&writer, crc);
    if (writer.overflowed) {
        return 0U;
    }
    return total;
}

bool config_store_deserialize(const uint8_t *in, uint32_t in_size,
                              board_security_config_t *config)
{
    reader_t reader;
    uint16_t payload_length;
    uint32_t expected_crc;
    uint32_t actual_crc;

    if ((in == NULL) || (config == NULL) ||
        (in_size < (CONFIG_STORE_HEADER_SIZE + CONFIG_STORE_CRC_SIZE))) {
        return false;
    }

    reader = (reader_t){in, in_size, 0U, false};
    if (get_u32(&reader) != CONFIG_STORE_MAGIC) {
        return false;
    }
    if (get_u16(&reader) != CONFIG_STORE_VERSION) {
        return false;
    }
    payload_length = get_u16(&reader);
    if (payload_length != (uint16_t)CONFIG_STORE_PAYLOAD_SIZE) {
        return false;
    }
    (void)get_u32(&reader); /* sequence，调用方按需另取 */

    if (in_size < (CONFIG_STORE_HEADER_SIZE + (uint32_t)payload_length +
                   CONFIG_STORE_CRC_SIZE)) {
        return false;
    }

    expected_crc = 0U;
    (void)memcpy(&expected_crc, &in[CONFIG_STORE_HEADER_SIZE + payload_length],
                 sizeof(expected_crc));
    actual_crc = event_log_crc32(in, CONFIG_STORE_HEADER_SIZE +
                                     (uint32_t)payload_length);
    if (expected_crc != actual_crc) {
        return false;
    }

    return config_decode(&reader, config);
}

/* 读一个槽：返回信封是否合法，并输出序号。 */
static bool slot_read(const config_store_media_t *media, uint32_t address,
                      board_security_config_t *config, uint32_t *sequence)
{
    uint8_t header[CONFIG_STORE_HEADER_SIZE];
    uint8_t body[CONFIG_STORE_PAYLOAD_SIZE + CONFIG_STORE_CRC_SIZE];
    uint8_t envelope[CONFIG_STORE_ENVELOPE_MAX_SIZE];
    uint32_t envelope_length;

    if ((media->read(media->context, address, header, sizeof(header))) == false) {
        return false;
    }
    if ((header[0] != (uint8_t)(CONFIG_STORE_MAGIC & 0xFFU)) ||
        (header[1] != (uint8_t)((CONFIG_STORE_MAGIC >> 8U) & 0xFFU)) ||
        (header[2] != (uint8_t)((CONFIG_STORE_MAGIC >> 16U) & 0xFFU)) ||
        (header[3] != (uint8_t)((CONFIG_STORE_MAGIC >> 24U) & 0xFFU))) {
        return false;
    }

    envelope_length = CONFIG_STORE_HEADER_SIZE + CONFIG_STORE_PAYLOAD_SIZE +
                      CONFIG_STORE_CRC_SIZE;
    if (envelope_length > sizeof(envelope)) {
        return false;
    }
    if (!media->read(media->context, address + CONFIG_STORE_HEADER_SIZE, body,
                     sizeof(body))) {
        return false;
    }

    (void)memcpy(envelope, header, sizeof(header));
    (void)memcpy(&envelope[CONFIG_STORE_HEADER_SIZE], body, sizeof(body));

    if (!config_store_deserialize(envelope, envelope_length, config)) {
        return false;
    }

    *sequence = (uint32_t)header[8] | ((uint32_t)header[9] << 8U) |
                ((uint32_t)header[10] << 16U) | ((uint32_t)header[11] << 24U);
    return true;
}

bool config_store_load(const config_store_media_t *media,
                       board_security_config_t *config)
{
    board_security_config_t slot_a;
    board_security_config_t slot_b;
    uint32_t sequence_a = 0U;
    uint32_t sequence_b = 0U;
    const bool valid_a = (media != NULL) && (config != NULL) &&
                         slot_read(media, media->slot_a_address, &slot_a,
                                   &sequence_a);
    const bool valid_b = (media != NULL) && (config != NULL) &&
                         slot_read(media, media->slot_b_address, &slot_b,
                                   &sequence_b);

    if (media == NULL) {
        return false;
    }
    if (valid_a && valid_b) {
        *config = ((int32_t)(sequence_a - sequence_b) >= 0) ? slot_a : slot_b;
        return true;
    }
    if (valid_a) {
        *config = slot_a;
        return true;
    }
    if (valid_b) {
        *config = slot_b;
        return true;
    }
    return false;
}

bool config_store_save(const config_store_media_t *media,
                       const board_security_config_t *config)
{
    board_security_config_t existing;
    uint8_t envelope[CONFIG_STORE_ENVELOPE_MAX_SIZE];
    uint32_t envelope_length;
    uint32_t target_address;
    uint32_t next_sequence;
    uint32_t sequence_a = 0U;
    uint32_t sequence_b = 0U;
    const bool valid_a = slot_read(media, media->slot_a_address, &existing,
                                   &sequence_a);
    const bool valid_b = slot_read(media, media->slot_b_address, &existing,
                                   &sequence_b);

    if ((media == NULL) || (config == NULL)) {
        return false;
    }

    if (!valid_a && !valid_b) {
        /* 首次写入：落在 A 槽，序号从 1 开始 */
        target_address = media->slot_a_address;
        next_sequence = 1U;
    } else if (valid_a && valid_b) {
        /* 序号大的那个是活动槽，写另一个 */
        if ((int32_t)(sequence_a - sequence_b) >= 0) {
            target_address = media->slot_b_address;
            next_sequence = sequence_a + 1U;
        } else {
            target_address = media->slot_a_address;
            next_sequence = sequence_b + 1U;
        }
    } else if (valid_a) {
        target_address = media->slot_b_address;
        next_sequence = sequence_a + 1U;
    } else {
        target_address = media->slot_a_address;
        next_sequence = sequence_b + 1U;
    }

    envelope_length = config_store_serialize(config, envelope, sizeof(envelope));
    if (envelope_length == 0U) {
        return false;
    }
    /* 回填序号（offset 8，小端） */
    envelope[8] = (uint8_t)(next_sequence & 0xFFU);
    envelope[9] = (uint8_t)((next_sequence >> 8U) & 0xFFU);
    envelope[10] = (uint8_t)((next_sequence >> 16U) & 0xFFU);
    envelope[11] = (uint8_t)((next_sequence >> 24U) & 0xFFU);
    {
        uint32_t crc;
        (void)memcpy(&crc, &envelope[envelope_length - CONFIG_STORE_CRC_SIZE],
                     sizeof(crc));
        crc = event_log_crc32(envelope,
                              envelope_length - CONFIG_STORE_CRC_SIZE);
        (void)memcpy(&envelope[envelope_length - CONFIG_STORE_CRC_SIZE], &crc,
                     sizeof(crc));
    }

    /* 先擦目标扇区（目标槽是"旧的"那份，擦掉它不会损失最新配置），
     * 再整段写入。任一步失败都保留另一个槽可读。 */
    if (!media->erase(media->context, target_address)) {
        return false;
    }
    if (!media->write(media->context, target_address, envelope,
                      envelope_length)) {
        return false;
    }

    {
        board_security_config_t verify;
        uint32_t sequence = 0U;
        if (!slot_read(media, target_address, &verify, &sequence)) {
            return false;
        }
        return (sequence == next_sequence);
    }
}
