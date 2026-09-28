#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "event_log.h"
#include "base64.h"
#include "board_port.h"
#include "config_store.h"
#include "ds3231.h"
#include "keypad_input.h"
#include "lock_auth.h"
#include "lock_controller.h"
#include "onenet_token.h"
#include "pin_credential.h"
#include "periodic_credential.h"
#include "remote_command.h"
#include "sha1.h"
#include "totp.h"
#include "visitor_credential.h"

static int failures = 0;

#define CHECK(expression) do { \
    if (!(expression)) { \
        (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (0)

static void test_sha1(void)
{
    static const uint8_t expected[SHA1_DIGEST_SIZE] = {
        0xA9, 0x99, 0x3E, 0x36, 0x47, 0x06, 0x81, 0x6A, 0xBA, 0x3E,
        0x25, 0x71, 0x78, 0x50, 0xC2, 0x6C, 0x9C, 0xD0, 0xD8, 0x9D
    };
    uint8_t digest[SHA1_DIGEST_SIZE];
    sha1_digest((const uint8_t *)"abc", 3U, digest);
    CHECK(lock_constant_time_equal(digest, expected, sizeof(expected)));
}

typedef struct {
    uint8_t registers[0x20];
} rtc_mock_t;

static bool rtc_read(void *context, uint8_t reg, uint8_t *data, size_t length)
{
    rtc_mock_t *rtc = (rtc_mock_t *)context;
    if (((size_t)reg + length) > sizeof(rtc->registers)) {
        return false;
    }
    memcpy(data, &rtc->registers[reg], length);
    return true;
}

static bool rtc_write(void *context, uint8_t reg, const uint8_t *data,
                      size_t length)
{
    rtc_mock_t *rtc = (rtc_mock_t *)context;
    if (((size_t)reg + length) > sizeof(rtc->registers)) {
        return false;
    }
    memcpy(&rtc->registers[reg], data, length);
    return true;
}

static void test_ds3231(void)
{
    rtc_mock_t mock = {0};
    const ds3231_t rtc = {&mock, rtc_read, rtc_write};
    ds3231_datetime_t datetime;
    uint64_t unix_time;
    bool trusted;

    mock.registers[0] = 0x56U;
    mock.registers[1] = 0x34U;
    mock.registers[2] = 0x12U;
    mock.registers[3] = 0x04U;
    mock.registers[4] = 0x29U;
    mock.registers[5] = 0x02U;
    mock.registers[6] = 0x24U;
    CHECK(ds3231_read_datetime(&rtc, &datetime, &trusted));
    CHECK(trusted);
    CHECK(ds3231_datetime_to_unix(&datetime, &unix_time));
    CHECK(unix_time == UINT64_C(1709210096));

    mock.registers[0x0F] = 0x80U;
    CHECK(!ds3231_read_unix(&rtc, &unix_time));
    CHECK(ds3231_set_datetime(&rtc, &datetime));
    CHECK((mock.registers[0x0F] & 0x80U) == 0U);
    CHECK(ds3231_read_unix(&rtc, &unix_time));
}

static void test_totp_rfc6238(void)
{
    static const uint8_t secret[] = "12345678901234567890";
    static const struct {
        uint64_t timestamp;
        uint32_t expected;
    } vectors[] = {
        {UINT64_C(59), UINT32_C(94287082)},
        {UINT64_C(1111111109), UINT32_C(7081804)},
        {UINT64_C(1111111111), UINT32_C(14050471)},
        {UINT64_C(1234567890), UINT32_C(89005924)},
        {UINT64_C(2000000000), UINT32_C(69279037)},
        {UINT64_C(20000000000), UINT32_C(65353130)}
    };

    for (size_t i = 0U; i < (sizeof(vectors) / sizeof(vectors[0])); ++i) {
        CHECK(totp_generate(secret, sizeof(secret) - 1U, vectors[i].timestamp,
                            30U, 8U) == vectors[i].expected);
    }
    CHECK(totp_verify(secret, sizeof(secret) - 1U, 59U, 30U, 8U,
                      UINT32_C(94287082), 1U));
    CHECK(!totp_verify(secret, sizeof(secret) - 1U, 120U, 30U, 8U,
                       UINT32_C(94287082), 1U));
}

static void test_pin_and_lockout(void)
{
    static const uint8_t secret[20] = {
        0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U,
        10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U
    };
    lock_auth_state_t state = {0};
    const lock_auth_policy_t policy = {3U, 60U};
    pin_credential_t credential;

    CHECK(pin_credential_set(&credential, secret, sizeof(secret), "123456", 6U));
    CHECK(pin_credential_verify(&credential, secret, sizeof(secret),
                                "9912345677", 10U, true, 100U, &policy,
                                &state) == LOCK_AUTH_GRANTED);
    CHECK(pin_credential_verify(&credential, secret, sizeof(secret), "000000",
                                6U, true, 101U, &policy,
                                &state) == LOCK_AUTH_DENIED);
    CHECK(pin_credential_verify(&credential, secret, sizeof(secret), "000000",
                                6U, true, 102U, &policy,
                                &state) == LOCK_AUTH_DENIED);
    CHECK(pin_credential_verify(&credential, secret, sizeof(secret), "000000",
                                6U, true, 103U, &policy,
                                &state) == LOCK_AUTH_LOCKED);
    CHECK(pin_credential_verify(&credential, secret, sizeof(secret), "123456",
                                6U, true, 120U, &policy,
                                &state) == LOCK_AUTH_LOCKED);
    CHECK(pin_credential_verify(&credential, secret, sizeof(secret), "123456",
                                6U, true, 164U, &policy,
                                &state) == LOCK_AUTH_GRANTED);
}

static void test_visitor_code(void)
{
    static const uint8_t secret[20] = {
        1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U,
        11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U
    };
    visitor_credential_t credential;

    CHECK(visitor_credential_issue(&credential, secret, sizeof(secret),
                                   "654321", 6U, 100U, 400U, 1U));
    CHECK(!visitor_credential_verify_and_consume(&credential, secret,
                                                 sizeof(secret), "000000", 6U,
                                                 200U));
    CHECK(visitor_credential_verify_and_consume(&credential, secret,
                                                sizeof(secret), "654321", 6U,
                                                200U));
    CHECK(!visitor_credential_verify_and_consume(&credential, secret,
                                                 sizeof(secret), "654321", 6U,
                                                 201U));
}

static void test_periodic_pin(void)
{
    static const uint8_t secret[20] = {
        1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U,
        11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U
    };
    periodic_credential_t credential;

    CHECK(periodic_credential_set(&credential, secret, sizeof(secret),
                                  "135790", 6U, UINT8_C(0x01),
                                  600U, 660U, 0));
    /* 1970-01-05 是周一：10:30 落在已配置的时间窗内。 */
    CHECK(periodic_credential_verify(&credential, secret, sizeof(secret),
                                     "9913579000", 10U, true,
                                     UINT64_C(383400)));
    CHECK(!periodic_credential_verify(&credential, secret, sizeof(secret),
                                      "135790", 6U, true,
                                      UINT64_C(385200)));
    CHECK(!periodic_credential_verify(&credential, secret, sizeof(secret),
                                      "135790", 6U, true,
                                      UINT64_C(469800)));
}

static void test_remote_command(void)
{
    static const uint8_t secret[32] = {
        0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U,
        8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U,
        16U, 17U, 18U, 19U, 20U, 21U, 22U, 23U,
        24U, 25U, 26U, 27U, 28U, 29U, 30U, 31U
    };
    static const uint8_t visitor_expected_tag[SHA1_DIGEST_SIZE] = {
        0xCFU, 0x56U, 0x16U, 0xCCU, 0xEBU, 0x16U, 0xB0U, 0x8CU,
        0xA9U, 0xAAU, 0xCEU, 0xA8U, 0xEAU, 0xE3U, 0x8EU, 0xBAU,
        0xA5U, 0x9EU, 0xC3U, 0x7FU
    };
    remote_command_t command = {
        .request_id = UINT64_C(42),
        .issued_at = UINT64_C(1000),
        .expires_at = UINT64_C(1030),
        .action = REMOTE_ACTION_UNLOCK,
        .authentication_tag = {0}
    };
    uint64_t last_request = 0U;

    CHECK(remote_command_sign(&command, secret, sizeof(secret)));
    CHECK(remote_command_verify(&command, secret, sizeof(secret), 1005U, 5U,
                                &last_request) == REMOTE_COMMAND_ACCEPTED);
    CHECK(last_request == 42U);
    CHECK(remote_command_verify(&command, secret, sizeof(secret), 1006U, 5U,
                                &last_request) == REMOTE_COMMAND_REPLAYED);
    command.request_id = 43U;
    CHECK(remote_command_sign(&command, secret, sizeof(secret)));
    command.authentication_tag[0] ^= 1U;
    CHECK(remote_command_verify(&command, secret, sizeof(secret), 1006U, 5U,
                                &last_request) == REMOTE_COMMAND_BAD_TAG);

    memset(&command, 0, sizeof(command));
    command.request_id = 44U;
    command.issued_at = 1010U;
    command.expires_at = 1310U;
    command.action = REMOTE_ACTION_ISSUE_VISITOR;
    memcpy(command.visitor_code, "246810", 6U);
    command.visitor_code_length = 6U;
    command.visitor_uses = 1U;
    CHECK(remote_command_sign(&command, secret, sizeof(secret)));
    CHECK(lock_constant_time_equal(command.authentication_tag,
                                   visitor_expected_tag,
                                   sizeof(visitor_expected_tag)));
    command.visitor_code[0] = '9';
    CHECK(remote_command_verify(&command, secret, sizeof(secret), 1011U, 5U,
                                &last_request) == REMOTE_COMMAND_BAD_TAG);
}

#define MEMORY_LOG_SLOTS 4U
typedef struct {
    event_log_record_t slots[MEMORY_LOG_SLOTS];
    bool written[MEMORY_LOG_SLOTS];
} memory_log_t;

static bool memory_read(void *context, uint32_t slot, event_log_record_t *record)
{
    memory_log_t *memory = (memory_log_t *)context;
    if ((slot >= MEMORY_LOG_SLOTS) || (!memory->written[slot])) {
        return false;
    }
    *record = memory->slots[slot];
    return true;
}

static bool memory_write(void *context, uint32_t slot,
                         const event_log_record_t *record)
{
    memory_log_t *memory = (memory_log_t *)context;
    if (slot >= MEMORY_LOG_SLOTS) {
        return false;
    }
    memory->slots[slot] = *record;
    memory->written[slot] = true;
    return true;
}

static bool memory_erase(void *context, uint32_t slot)
{
    memory_log_t *memory = (memory_log_t *)context;
    if (slot >= MEMORY_LOG_SLOTS) {
        return false;
    }
    memset(&memory->slots[slot], 0xFF, sizeof(memory->slots[slot]));
    memory->written[slot] = false;
    return true;
}

static void test_event_log(void)
{
    memory_log_t memory = {0};
    const event_log_storage_t storage = {
        .context = &memory,
        .slot_count = MEMORY_LOG_SLOTS,
        .read = memory_read,
        .write = memory_write,
        .erase = memory_erase
    };
    event_log_t log;
    event_log_t recovered;
    event_log_record_t record = {
        .unix_time = 1000U,
        .user_id = 7U,
        .event_type = LOCK_EVENT_UNLOCK,
        .auth_method = LOCK_METHOD_TOTP,
        .result = 1U
    };

    CHECK(event_log_init(&log, &storage));
    for (uint32_t i = 0U; i < 6U; ++i) {
        record.unix_time = 1000U + i;
        CHECK(event_log_append(&log, &record));
    }
    CHECK(log.next_sequence == 7U);
    CHECK(event_log_init(&recovered, &storage));
    CHECK(recovered.next_sequence == 7U);
    CHECK(recovered.next_slot == 2U);
    CHECK(recovered.valid_count == MEMORY_LOG_SLOTS);
    CHECK(event_log_read_recent(&recovered, 0U, &record));
    CHECK(record.sequence == 6U);
    CHECK(event_log_read_recent(&recovered, 3U, &record));
    CHECK(record.sequence == 3U);
    CHECK(!event_log_read_recent(&recovered, 4U, &record));
}

static void test_controller(void)
{
    lock_controller_t controller;
    lock_controller_init(&controller, 5U);
    CHECK(lock_controller_handle(&controller, LOCK_CONTROL_AUTH_GRANTED, 100U) ==
          LOCK_STATE_UNLOCKED);
    CHECK(lock_controller_handle(&controller, LOCK_CONTROL_TICK, 104U) ==
          LOCK_STATE_UNLOCKED);
    CHECK(lock_controller_handle(&controller, LOCK_CONTROL_TICK, 105U) ==
          LOCK_STATE_LOCKED);
    CHECK(lock_controller_handle(&controller, LOCK_CONTROL_TAMPER, 106U) ==
          LOCK_STATE_ALARM);
    CHECK(lock_controller_handle(&controller, LOCK_CONTROL_AUTH_GRANTED, 107U) ==
          LOCK_STATE_ALARM);
    CHECK(lock_controller_handle(&controller, LOCK_CONTROL_CLEAR_ALARM, 108U) ==
          LOCK_STATE_LOCKED);
    lock_controller_set_lockout(&controller, 200U);
    controller.door_closed = false;
    CHECK(lock_controller_handle(&controller, LOCK_CONTROL_CLEAR_LOCKOUT,
                                 109U) == LOCK_STATE_LOCKED);
    CHECK(!controller.door_closed);
}

/* ------------------------------------------------------------------ */
/* base64 编解码                                                             */
/* ------------------------------------------------------------------ */
static void test_base64(void)
{
    uint8_t decoded[64];
    char encoded[128];

    CHECK(base64_encode((const uint8_t *)"abc", 3U, encoded) == 4U);
    CHECK(strcmp(encoded, "YWJj") == 0);
    CHECK(base64_encode((const uint8_t *)"ab", 2U, encoded) == 4U);
    CHECK(strcmp(encoded, "YWI=") == 0);
    CHECK(base64_encode((const uint8_t *)"a", 1U, encoded) == 4U);
    CHECK(strcmp(encoded, "YQ==") == 0);
    CHECK(base64_encode((const uint8_t *)"", 0U, encoded) == 0U);

    CHECK(base64_decode("YWJj", 4U, decoded) == 3U);
    CHECK(memcmp(decoded, "abc", 3U) == 0);
    CHECK(base64_decode("YQ==", 4U, decoded) == 1U);
    CHECK(decoded[0] == (uint8_t)'a');
    /* 噪声（换行/空格）应被忽略 */
    CHECK(base64_decode("YW\r\nJj", 6U, decoded) == 3U);
    CHECK(memcmp(decoded, "abc", 3U) == 0);

    /* 任意二进制往返 */
    {
        uint8_t source[20];
        uint8_t restored[32];
        for (uint8_t i = 0U; i < (uint8_t)sizeof(source); ++i) {
            source[i] = (uint8_t)(i * 11U + 7U);
        }
        CHECK(base64_encode(source, sizeof(source), encoded) == 28U);
        CHECK(base64_decode(encoded, strlen(encoded), restored) == sizeof(source));
        CHECK(memcmp(source, restored, sizeof(source)) == 0);
    }
}

/* ------------------------------------------------------------------ */
/* OneNET 鉴权令牌                                                       */
/* ------------------------------------------------------------------ */
static void test_onenet_token(void)
{
    char token[256];
    char again[256];
    uint16_t length;

    /* 三元组取自参考工程 HARDWARE/esp8266.h，必须与之一致。 */
    static const onenet_cred_t credential = {
        "v4nyue7h41",
        "smartlock_001",
        "QmdPMGFia2c3Ym84UU1ONW5XNXk3WEc0SklHVjdHMVo="
    };

    length = onenet_calc_token(&credential, UINT32_C(1700000000),
                              token, (uint16_t)sizeof(token));
    CHECK(length > 0U);
    CHECK(strncmp(token, "version=2018-10-31&res=products%2Fv4nyue7h41",
                  43U) == 0);
    CHECK(strstr(token, "&et=1700000000&method=sha1&sign=") != NULL);

    /* 确定性：同样输入必须得到同样输出 */
    CHECK(onenet_calc_token(&credential, UINT32_C(1700000000), again,
                            (uint16_t)sizeof(again)) == length);
    CHECK(strcmp(token, again) == 0);

    /* et 变化必须改变签名。注意：不能断言两次长度相等——sign 是 base64，
     * 其中出现的 '+' 与 '/' 会被转义成 3 个字符，因此总长与签名内容有关。 */
    CHECK(onenet_calc_token(&credential, UINT32_C(1700000001), again,
                            (uint16_t)sizeof(again)) > 0U);
    CHECK(strcmp(token, again) != 0);

    /* 参数非法 */
    CHECK(onenet_calc_token(NULL, 1U, token, (uint16_t)sizeof(token)) == 0U);
    CHECK(onenet_calc_token(&credential, 1U, token, 8U) == 0U);
}

/* ------------------------------------------------------------------ */
/* 配置存储 config_store                                                       */
/* ------------------------------------------------------------------ */
#define CONFIG_TEST_SLOT_SIZE 4096U

typedef struct {
    uint8_t slot_a[CONFIG_TEST_SLOT_SIZE];
    uint8_t slot_b[CONFIG_TEST_SLOT_SIZE];
    uint32_t write_count;
    bool fail_erase;
    bool fail_write;
} config_media_mock_t;

static bool config_media_read(void *context, uint32_t offset, uint8_t *data,
                              uint32_t length)
{
    config_media_mock_t *mock = (config_media_mock_t *)context;
    uint8_t *base = NULL;

    if (offset >= CONFIG_TEST_SLOT_SIZE) {
        base = mock->slot_b;
        offset -= CONFIG_TEST_SLOT_SIZE;
    } else {
        base = mock->slot_a;
    }
    if ((offset + length) > CONFIG_TEST_SLOT_SIZE) {
        return false;
    }
    (void)memcpy(data, &base[offset], length);
    return true;
}

static bool config_media_write(void *context, uint32_t offset,
                               const uint8_t *data, uint32_t length)
{
    config_media_mock_t *mock = (config_media_mock_t *)context;
    uint8_t *base = NULL;

    if (mock->fail_write) {
        return false;
    }
    if (offset >= CONFIG_TEST_SLOT_SIZE) {
        base = mock->slot_b;
        offset -= CONFIG_TEST_SLOT_SIZE;
    } else {
        base = mock->slot_a;
    }
    if ((offset + length) > CONFIG_TEST_SLOT_SIZE) {
        return false;
    }
    (void)memcpy(&base[offset], data, length);
    ++mock->write_count;
    return true;
}

static bool config_media_erase(void *context, uint32_t address)
{
    config_media_mock_t *mock = (config_media_mock_t *)context;
    uint8_t *base = NULL;

    if (mock->fail_erase) {
        return false;
    }
    if (address >= CONFIG_TEST_SLOT_SIZE) {
        base = mock->slot_b;
    } else {
        base = mock->slot_a;
    }
    (void)memset(base, 0xFF, CONFIG_TEST_SLOT_SIZE);
    return true;
}

static void make_test_config(board_security_config_t *config, uint8_t marker)
{
    static const uint8_t secret[20] = {
        1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U,
        11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U
    };

    (void)memset(config, 0, sizeof(*config));
    for (size_t i = 0U; i < sizeof(config->device_secret); ++i) {
        config->device_secret[i] = (uint8_t)(marker + (uint8_t)i);
    }
    for (size_t i = 0U; i < sizeof(config->totp_secret); ++i) {
        config->totp_secret[i] = marker;
    }
    config->totp_secret_length = 20U;
    CHECK(pin_credential_set(&config->owner_pin, secret, sizeof(secret),
                             "123456", 6U));
    CHECK(pin_credential_set(&config->duress_pin, secret, sizeof(secret),
                             "654321", 6U));
    config->periodic_pin.enabled = false;
    config->visitor.active = false;
    config->last_remote_request_id = (uint64_t)marker * 100U;
    config->second_factor_mode = BOARD_SECOND_FACTOR_DISABLED;
}

static void test_config_store(void)
{
    static config_media_mock_t mock;
    static board_security_config_t config;
    static board_security_config_t loaded;
    uint8_t envelope[CONFIG_STORE_ENVELOPE_MAX_SIZE];
    uint32_t envelope_length;
    const config_store_media_t media = {
        .context = &mock,
        .slot_a_address = 0U,
        .slot_b_address = CONFIG_TEST_SLOT_SIZE,
        .read = config_media_read,
        .write = config_media_write,
        .erase = config_media_erase
    };

    (void)memset(&mock, 0, sizeof(mock));
    (void)memset(mock.slot_a, 0xFF, sizeof(mock.slot_a));
    (void)memset(mock.slot_b, 0xFF, sizeof(mock.slot_b));

    /* 空 Flash：读不出来 */
    CHECK(!config_store_load(&media, &loaded));

    /* 序列化 / 反序列化往返 */
    make_test_config(&config, 0x10U);
    envelope_length = config_store_serialize(&config, envelope,
                                             sizeof(envelope));
    CHECK(envelope_length ==
          (CONFIG_STORE_HEADER_SIZE + CONFIG_STORE_PAYLOAD_SIZE +
           CONFIG_STORE_CRC_SIZE));
    CHECK(config_store_deserialize(envelope, envelope_length, &loaded));
    CHECK(memcmp(&config, &loaded, sizeof(config)) == 0);

    /* 单字节损坏必须被 CRC 挡住 */
    envelope[CONFIG_STORE_HEADER_SIZE + 3U] ^= 0x01U;
    CHECK(!config_store_deserialize(envelope, envelope_length, &loaded));

    /* 版本号不符必须被拒 */
    envelope_length = config_store_serialize(&config, envelope,
                                             sizeof(envelope));
    envelope[4] = 0x7FU;
    CHECK(!config_store_deserialize(envelope, envelope_length, &loaded));

    /* 首次保存落在 A 槽，序号 1 */
    CHECK(config_store_save(&media, &config));
    CHECK(config_store_load(&media, &loaded));
    CHECK(memcmp(&config, &loaded, sizeof(config)) == 0);

    /* 第二次保存切到 B 槽（A 槽保持可读直到 B 写完） */
    make_test_config(&config, 0x20U);
    CHECK(config_store_save(&media, &config));
    CHECK(config_store_load(&media, &loaded));
    CHECK(loaded.device_secret[0] == (uint8_t)(0x20U));

    /* 掉电模拟：擦除失败 → 保存失败，但旧配置仍可读回 */
    make_test_config(&config, 0x30U);
    mock.fail_erase = true;
    CHECK(!config_store_save(&media, &config));
    mock.fail_erase = false;
    CHECK(config_store_load(&media, &loaded));
    CHECK(loaded.device_secret[0] == (uint8_t)(0x20U));

    /* 掉电模拟：写入失败 → 同样保留旧配置 */
    mock.fail_write = true;
    CHECK(!config_store_save(&media, &config));
    mock.fail_write = false;
    CHECK(config_store_load(&media, &loaded));
    CHECK(loaded.device_secret[0] == (uint8_t)(0x20U));

    /* A 槽被擦成 0xFF 后，只剩 B 槽也能正常读 */
    (void)memset(mock.slot_a, 0xFF, sizeof(mock.slot_a));
    CHECK(config_store_load(&media, &loaded));
    CHECK(loaded.device_secret[0] == (uint8_t)(0x20U));
}

/* ------------------------------------------------------------------ */
/* 键盘矩阵扫描                                                        */
/* ------------------------------------------------------------------ */
/* 虚拟矩阵：把"哪个键被按住"映射成行选中 + 列读回 */
typedef struct {
    uint8_t selected_row;   /* KEYPAD_ROWS 表示全部取消 */
    uint8_t active_row;     /* 当前被按住的键所在行 */
    uint8_t active_column;  /* 当前被按住的键所在列 */
    bool pressed;
} keypad_mock_t;

static void keypad_mock_select_row(void *context, uint8_t row)
{
    ((keypad_mock_t *)context)->selected_row = row;
}

static bool keypad_mock_read_column(void *context, uint8_t column)
{
    const keypad_mock_t *mock = (const keypad_mock_t *)context;
    if (!mock->pressed) {
        return false;
    }
    return (mock->selected_row == mock->active_row) &&
           (column == mock->active_column);
}

static void test_keypad_matrix(void)
{
    keypad_mock_t mock = {KEYPAD_ROWS, 0U, 0U, false};
    const keypad_matrix_io_t io = {
        .context = &mock,
        .select_row = keypad_mock_select_row,
        .read_column = keypad_mock_read_column
    };
    keypad_scan_t scan;

    keypad_scan_init(&scan);

    /* 无按键：不产生事件，并且扫描结束后行驱动应被取消选中 */
    CHECK(keypad_scan_step(&io, &scan) == '\0');
    CHECK(mock.selected_row == (uint8_t)KEYPAD_ROWS);

    /* 按下 '5'（第 2 行第 2 列）：需要两拍消抖后才上报，且只上报一次 */
    mock.pressed = true;
    mock.active_row = 1U;
    mock.active_column = 1U;
    CHECK(keypad_scan_step(&io, &scan) == '\0');
    CHECK(keypad_scan_step(&io, &scan) == '5');
    CHECK(keypad_scan_step(&io, &scan) == '\0'); /* 长按不重复 */
    CHECK(keypad_scan_step(&io, &scan) == '\0');

    /* 抬起：两拍后解除确认 */
    mock.pressed = false;
    CHECK(keypad_scan_step(&io, &scan) == '\0');
    CHECK(keypad_scan_step(&io, &scan) == '\0');

    /* 再按同一个键应再次上报 */
    mock.pressed = true;
    CHECK(keypad_scan_step(&io, &scan) == '\0');
    CHECK(keypad_scan_step(&io, &scan) == '5');

    /* 换键：'#'（第 4 行第 3 列）需要重新消抖 */
    mock.active_row = 3U;
    mock.active_column = 2U;
    CHECK(keypad_scan_step(&io, &scan) == '\0');
    CHECK(keypad_scan_step(&io, &scan) == '#');
}

/* ------------------------------------------------------------------ */
/* 键盘序列 → 输入事件                                                  */
/* ------------------------------------------------------------------ */
static void feed_digits(keypad_input_t *input, const char *digits,
                        uint32_t *clock)
{
    for (size_t i = 0U; digits[i] != '\0'; ++i) {
        board_input_event_t event;
        *clock += 100U;
        CHECK(!keypad_input_feed(input, digits[i], *clock, &event));
    }
}

static void test_keypad_input(void)
{
    keypad_input_t input;
    board_input_event_t event;
    uint32_t clock = 1000U;

    /* 普通密码：6 位 + '#' 确认 */
    keypad_input_init(&input);
    feed_digits(&input, "123456", &clock);
    CHECK(keypad_input_length(&input) == 6U);
    ++clock;
    CHECK(keypad_input_feed(&input, '#', clock, &event));
    CHECK(event.type == BOARD_AUTH_PIN);
    CHECK(event.digit_count == 6U);
    CHECK(memcmp(event.digits, "123456", 6U) == 0);
    CHECK(keypad_input_length(&input) == 0U);

    /* 虚位密码：前缀乱码 + 正确密码，'#' 时原样上报整串，
     * 由 pin_credential_matches(..., allow_virtual=true) 负责切分 */
    keypad_input_init(&input);
    feed_digits(&input, "9912345677", &clock);
    ++clock;
    CHECK(keypad_input_feed(&input, '#', clock, &event));
    CHECK(event.type == BOARD_AUTH_PIN);
    CHECK(event.digit_count == 10U);
    CHECK(memcmp(event.digits, "9912345677", 10U) == 0);

    /* '*' 前缀切换成 TOTP 动态口令 */
    keypad_input_init(&input);
    ++clock;
    CHECK(!keypad_input_feed(&input, '*', clock, &event));
    feed_digits(&input, "654321", &clock);
    ++clock;
    CHECK(keypad_input_feed(&input, '#', clock, &event));
    CHECK(event.type == BOARD_AUTH_TOTP);
    CHECK(event.digit_count == 6U);

    /* 已有输入时按 '*' 视为取消 */
    keypad_input_init(&input);
    feed_digits(&input, "12", &clock);
    ++clock;
    CHECK(!keypad_input_feed(&input, '*', clock, &event));
    CHECK(keypad_input_length(&input) == 0U);
    ++clock;
    CHECK(!keypad_input_feed(&input, '#', clock, &event)); /* 空输入不产生事件 */

    /* 日志翻页 */
    keypad_input_init(&input);
    ++clock;
    CHECK(keypad_input_feed(&input, 'A', clock, &event));
    CHECK(event.type == BOARD_SHOW_RECENT_LOG);
    CHECK(event.user_id == 0U);
    ++clock;
    CHECK(keypad_input_feed(&input, 'B', clock, &event));
    CHECK(event.user_id == 1U);
    ++clock;
    CHECK(keypad_input_feed(&input, 'B', clock, &event));
    CHECK(event.user_id == 2U);
    ++clock;
    CHECK(keypad_input_feed(&input, 'C', clock, &event)); /* 'C' 取消：产出事件 */
    CHECK(event.type == BOARD_KEYPAD_CANCEL);
    CHECK(event.digit_count == 0U);
    ++clock;
    CHECK(keypad_input_feed(&input, 'A', clock, &event));
    CHECK(event.user_id == 0U); /* 游标已复位 */

    /* 'D' 进管理向导（改主密码）：产出 BOARD_MENU_REQUEST，并丢掉手上的半截数字，
     * 否则那几位会被当成向导第一步"原密码"的输入，用户会莫名其妙验证失败 */
    keypad_input_init(&input);
    feed_digits(&input, "12", &clock);
    CHECK(keypad_input_length(&input) == 2U);
    ++clock;
    CHECK(keypad_input_feed(&input, 'D', clock, &event));
    CHECK(event.type == BOARD_MENU_REQUEST);
    CHECK(event.digit_count == 0U);
    CHECK(keypad_input_length(&input) == 0U);

    /* 'C' 取消之后再按 '#'：空输入不产生事件，不会误发一次 BOARD_AUTH_PIN */
    keypad_input_init(&input);
    feed_digits(&input, "123456", &clock);
    ++clock;
    CHECK(keypad_input_feed(&input, 'C', clock, &event));
    CHECK(event.type == BOARD_KEYPAD_CANCEL);
    CHECK(keypad_input_length(&input) == 0U);
    ++clock;
    CHECK(!keypad_input_feed(&input, '#', clock, &event));

    /* 缓冲溢出：FIFO 丢弃最旧的一位，保证最后 32 位仍完整 */
    keypad_input_init(&input);
    feed_digits(&input, "0123456789012345678901234567890123456789", &clock);
    CHECK(keypad_input_length(&input) == (uint8_t)LOCK_INPUT_MAX_LENGTH);
    ++clock;
    CHECK(keypad_input_feed(&input, '#', clock, &event));
    CHECK(event.digit_count == (uint8_t)LOCK_INPUT_MAX_LENGTH);
    CHECK(memcmp(event.digits, "89012345678901234567890123456789",
                 LOCK_INPUT_MAX_LENGTH) == 0);

    /* 无操作超时清空缓冲 */
    keypad_input_init(&input);
    feed_digits(&input, "1234", &clock);
    CHECK(!keypad_input_tick(&input, clock + 1000U));
    CHECK(keypad_input_length(&input) == 4U);
    CHECK(keypad_input_tick(&input, clock + 9000U));
    CHECK(keypad_input_length(&input) == 0U);
}

int main(void)
{
    test_sha1();
    test_ds3231();
    test_totp_rfc6238();
    test_pin_and_lockout();
    test_visitor_code();
    test_periodic_pin();
    test_remote_command();
    test_event_log();
    test_controller();
    test_base64();
    test_onenet_token();
    test_config_store();
    test_keypad_matrix();
    test_keypad_input();

    if (failures == 0) {
        (void)printf("All smart-lock core tests passed.\n");
        return 0;
    }
    (void)printf("%d test(s) failed.\n", failures);
    return 1;
}
