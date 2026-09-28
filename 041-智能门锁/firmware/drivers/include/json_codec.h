/*
 * json_codec.h
 *
 * 轻量级 JSON 编解码器，专为 STM32 智能门锁场景设计：
 *  - 仅依赖 C11 标准头，不引入 cJSON；
 *  - 使用顺序写入的编码器，避免堆分配；
 *  - 使用 schema-driven 的解析器，把 JSON 输入压缩为若干「键路径 → 值」表。
 *
 * 协议约定见 protocol/mqtt.md：
 *  - 通知载荷字段：state, sequence, timestamp, event_type, auth_method,
 *    user_id, result；
 *  - 命令载荷字段：request_id, issued_at, expires_at, action, visitor_code,
 *    visitor_code_length, visitor_uses, authentication_tag。
 *
 * 本头文件与 .c 一同遵守「严格基于现有代码、不修改既有 API」的原则：
 * 它可以被 smart_lock_app.c 之外的便携式模块直接调用，不会触碰任何
 * 现有 portable API 的签名或语义。
 */
#ifndef SMART_LOCK_JSON_CODEC_H
#define SMART_LOCK_JSON_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 编码器：单次顺序写入，对内存占用和 flash 占用都很小。
 * 字节缓冲由调用方提供；写满时返回 false，但不会改写越界字节。 */
typedef struct {
    uint8_t *buffer;     /* 调用方提供的字节缓冲 */
    size_t capacity;     /* 缓冲总容量，字节 */
    size_t length;       /* 当前已写入字节数 */
    bool overflow;       /* 写超容量时记下，最终结果仍返回 false */
    bool need_separator; /* 是否需要在下一个值前插入逗号 */
} json_writer_t;

void json_writer_init(json_writer_t *writer, uint8_t *buffer, size_t capacity);
bool json_writer_begin_object(json_writer_t *writer);
bool json_writer_end_object(json_writer_t *writer);
bool json_writer_begin_array(json_writer_t *writer);
bool json_writer_end_array(json_writer_t *writer);
/* 写一个对象/数组字段下的对象值。例：
 *   json_writer_key(w, "command");
 *   json_writer_begin_object_value(w); ... json_writer_end_object_value(w); */
bool json_writer_begin_object_value(json_writer_t *writer);
bool json_writer_end_object_value(json_writer_t *writer);
bool json_writer_key(json_writer_t *writer, const char *key);

/* 标量字段：以 json_writer_key 写键名后，再用这些函数写值。 */
bool json_writer_int(json_writer_t *writer, int64_t value);
bool json_writer_bool(json_writer_t *writer, bool value);
bool json_writer_null(json_writer_t *writer);
/* 字符串字段：value_length 不包含结尾 NUL，传入 0 表示视为 NUL 终止字符串。 */
bool json_writer_string(json_writer_t *writer, const char *value,
                        size_t value_length);

/* 解析器：给定一段 buffer，按 schema 找到目标键路径并填充输出缓冲。
 *
 * schema 形如 "a.b.c"，a/b 是对象键，c 是叶子。叶子支持整型、布尔、字符串。
 * 字符串最长只能装到 out_string_capacity - 1 个字符，并以 NUL 结束。
 *
 * 输入容许在数字前后允许任意空白；键字符串不支持转义。整套限定对
 * 本项目 MQTT JSON 足够——命令/通知从来不会发送含控制字符的键。
 *
 * 解析失败时返回 false，并将 out_* 全部清零。 */
typedef enum {
    JSON_VALUE_NULL = 0,
    JSON_VALUE_BOOL,
    JSON_VALUE_INT,
    JSON_VALUE_STRING
} json_value_kind_t;

bool json_reader_find_int(const uint8_t *buffer, size_t length,
                          const char *path, int64_t *out_value);
bool json_reader_find_bool(const uint8_t *buffer, size_t length,
                           const char *path, bool *out_value);
bool json_reader_find_string(const uint8_t *buffer, size_t length,
                             const char *path, char *out_string,
                             size_t out_string_capacity,
                             size_t *out_string_length);

#endif
