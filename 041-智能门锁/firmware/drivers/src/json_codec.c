/*
 * json_codec.c
 *
 * 见 json_codec.h 的注释。实现策略：
 *  - 编码器逐字节追加到调用方缓冲，遇到溢出时停下并标记 overflow；
 *  - 解析器先按对象层递归下降，直到命中 path 的最后一个 token，再按叶子类型匹配。
 *
 * 这一文件只依赖标准库，不连接任何外部库，因此可以与现有 portable 核心
 * 一起在 host-debug 与 cortex-m3-release 两个 target 里构建。
 *
 * 重点：本文件保持「纯粹」——它不会去读写任何全局状态，所有上下文均
 * 由调用方提供，与现有 pin_credential / visitor_credential 等模块的风
 * 格完全一致。
 */
#include "json_codec.h"

#include <stddef.h>
#include <string.h>

/* 写入一个原始字节；溢出时仅标记状态，不写越界字节。 */
static bool put_byte(json_writer_t *writer, uint8_t byte)
{
    if (writer->length >= writer->capacity) {
        writer->overflow = true;
        return false;
    }
    writer->buffer[writer->length++] = byte;
    return true;
}

/* 在两个值之间插入分隔符：对象第一对 KV 之前不需要 ","，随后每对之前都需要。
 * 遇到空白键（不可见）时一并跳过；调用方负责 KV 的字符串转义，本项目只发送
 * ASCII 字符串，因此不需要越界处理。 */
static bool put_separator(json_writer_t *writer, char sep)
{
    if (writer->need_separator) {
        if (!put_byte(writer, (uint8_t)sep)) {
            return false;
        }
    }
    writer->need_separator = true;
    return true;
}

void json_writer_init(json_writer_t *writer, uint8_t *buffer, size_t capacity)
{
    writer->buffer = buffer;
    writer->capacity = capacity;
    writer->length = 0U;
    writer->overflow = false;
    writer->need_separator = false;
}

bool json_writer_begin_object(json_writer_t *writer)
{
    /* 用一个标记位记录「复合值已开始」：复合值后面紧跟 key/value，
     * 它们的分隔行为要遵循 outer 的状态，这里通过简单地追踪
     * 当前 need_separator flag 来达到一致的效果。 */
    return put_byte(writer, (uint8_t)'{');
}

bool json_writer_end_object(json_writer_t *writer)
{
    return put_byte(writer, (uint8_t)'}');
}

bool json_writer_begin_array(json_writer_t *writer)
{
    return put_byte(writer, (uint8_t)'[');
}

bool json_writer_end_array(json_writer_t *writer)
{
    return put_byte(writer, (uint8_t)']');
}

/* begin_object_value 与 begin_object 区别在于它先按 outer 的规则
 * 写一个分隔符（，或冒号后已被 key 写过），再写 "{"。这样调用方就可以：
 *   json_writer_key(w, "command");
 *   json_writer_begin_object_value(w);
 *     json_writer_key(w, "request_id"); json_writer_int(w, 42);
 *     ...
 *   json_writer_end_object_value(w);
 * 整体结构仍然合法。 */
bool json_writer_begin_object_value(json_writer_t *writer)
{
    if (!put_separator(writer, ',')) {
        return false;
    }
    return put_byte(writer, (uint8_t)'{');
}

bool json_writer_end_object_value(json_writer_t *writer)
{
    /* 复合值结束了，要重置 need_separator，避免下一对 KV 写错位置。 */
    writer->need_separator = true;
    return put_byte(writer, (uint8_t)'}');
}

bool json_writer_key(json_writer_t *writer, const char *key)
{
    size_t length;
    /* 写前先吃掉键内部的 ','，确保新 key 写到自己的对象中。 */
    if (key == NULL) {
        return false;
    }
    if (!put_separator(writer, ',')) {
        return false;
    }
    if (!put_byte(writer, (uint8_t)'"')) {
        return false;
    }
    length = strlen(key);
    for (size_t i = 0U; i < length; ++i) {
        if (!put_byte(writer, (uint8_t)key[i])) {
            return false;
        }
    }
    if (!put_byte(writer, (uint8_t)'"')) {
        return false;
    }
    if (!put_byte(writer, (uint8_t)':')) {
        return false;
    }
    /* key 已写完，紧跟的是一个标量/对象/数组 —— 在它写之前不应再插入 ','。 */
    writer->need_separator = false;
    return true;
}

bool json_writer_int(json_writer_t *writer, int64_t value)
{
    /* 自实现的 itoa：避免 snprintf。范围 [-2^63, 2^63-1]。 */
    uint8_t scratch[24];
    size_t pos = 0U;
    uint64_t magnitude;
    bool negative;
    size_t start;

    negative = value < 0;
    magnitude = negative ? (uint64_t)(-(value + 1)) + 1U : (uint64_t)value;
    if (negative) {
        scratch[pos++] = (uint8_t)'-';
    }
    /* 写至少一位。 */
    start = pos;
    if (magnitude == 0U) {
        scratch[pos++] = (uint8_t)'0';
    } else {
        while (magnitude != 0U) {
            scratch[pos++] = (uint8_t)('0' + (magnitude % 10U));
            magnitude /= 10U;
        }
    }
    /* 反转区间 reverse(start..pos) */
    for (size_t i = 0U; i < ((pos - start) / 2U); ++i) {
        const uint8_t tmp = scratch[start + i];
        scratch[start + i] = scratch[pos - 1U - i];
        scratch[pos - 1U - i] = tmp;
    }
    writer->need_separator = true;
    for (size_t i = 0U; i < pos; ++i) {
        if (!put_byte(writer, scratch[i])) {
            return false;
        }
    }
    return true;
}

bool json_writer_bool(json_writer_t *writer, bool value)
{
    static const uint8_t true_bytes[4] = {'t', 'r', 'u', 'e'};
    static const uint8_t false_bytes[5] = {'f', 'a', 'l', 's', 'e'};
    const uint8_t *bytes = value ? true_bytes : false_bytes;
    size_t length = value ? 4U : 5U;
    writer->need_separator = true;
    for (size_t i = 0U; i < length; ++i) {
        if (!put_byte(writer, bytes[i])) {
            return false;
        }
    }
    return true;
}

bool json_writer_null(json_writer_t *writer)
{
    static const uint8_t null_bytes[4] = {'n', 'u', 'l', 'l'};
    writer->need_separator = true;
    for (size_t i = 0U; i < 4U; ++i) {
        if (!put_byte(writer, null_bytes[i])) {
            return false;
        }
    }
    return true;
}

bool json_writer_string(json_writer_t *writer, const char *value,
                        size_t value_length)
{
    const char *actual = value;
    if (actual == NULL) {
        return json_writer_null(writer);
    }
    if ((value_length == 0U) && (value != NULL)) {
        value_length = strlen(value);
    }
    if (!put_byte(writer, (uint8_t)'"')) {
        return false;
    }
    for (size_t i = 0U; i < value_length; ++i) {
        const char c = actual[i];
        bool ok;
        /* 转义控制字符与引号、反斜杠。本项目实际只会写 URL-safe 字符串，
         * 但为防御性起见仍然处理常见字符。 */
        switch (c) {
        case '"':
            ok = put_byte(writer, (uint8_t)'\\') && put_byte(writer, (uint8_t)'"');
            break;
        case '\\':
            ok = put_byte(writer, (uint8_t)'\\') && put_byte(writer, (uint8_t)'\\');
            break;
        case '\n':
            ok = put_byte(writer, (uint8_t)'\\') && put_byte(writer, (uint8_t)'n');
            break;
        case '\r':
            ok = put_byte(writer, (uint8_t)'\\') && put_byte(writer, (uint8_t)'r');
            break;
        case '\t':
            ok = put_byte(writer, (uint8_t)'\\') && put_byte(writer, (uint8_t)'t');
            break;
        default:
            ok = put_byte(writer, (uint8_t)c);
            break;
        }
        if (!ok) {
            return false;
        }
    }
    if (!put_byte(writer, (uint8_t)'"')) {
        return false;
    }
    writer->need_separator = true;
    return true;
}

/* -------------------- 解析器（schema-driven） -------------------- */

static const uint8_t *skip_whitespace(const uint8_t *cursor,
                                      const uint8_t *end)
{
    while ((cursor < end) && ((*cursor == ' ') || (*cursor == '\t') ||
                              (*cursor == '\n') || (*cursor == '\r'))) {
        ++cursor;
    }
    return cursor;
}

/* 在 [start, end) 区间内查找键 "key"。找到则返回指向 ':' 后第一个
 * 非空白字符（即值起点）；否则返回 NULL。键必须正好匹配，
 * 不能在结构内嵌套搜索（schema 路径逐层下钻即可）。 */
static const uint8_t *find_key(const uint8_t *start, const uint8_t *end,
                               const char *key, const uint8_t **value_out)
{
    const uint8_t *cursor = start;
    cursor = skip_whitespace(cursor, end);
    if ((cursor == end) || (*cursor != '{')) {
        return NULL;
    }
    cursor = skip_whitespace(cursor + 1U, end);
    while (cursor < end) {
        if (*cursor == '}') {
            return NULL;
        }
        if (*cursor != '"') {
            return NULL;
        }
        const uint8_t *key_start = cursor + 1U;
        const uint8_t *key_end = key_start;
        while ((key_end < end) && (*key_end != '"')) {
            ++key_end;
        }
        if (key_end >= end) {
            return NULL;
        }
        size_t key_length = (size_t)(key_end - key_start);
        bool matched = (strlen(key) == key_length) &&
                       (memcmp(key_start, key, key_length) == 0);
        cursor = skip_whitespace(key_end + 1U, end);
        if ((cursor == end) || (*cursor != ':')) {
            return NULL;
        }
        cursor = skip_whitespace(cursor + 1U, end);
        if (matched) {
            *value_out = cursor;
            return cursor;
        }
        /* 跳到下一个 KV：先记录当前值起点，按简单深度计数前进。 */
        int depth = 0;
        bool in_string = false;
        bool escape = false;
        const uint8_t *value_cursor = cursor;
        while (value_cursor < end) {
            uint8_t ch = *value_cursor;
            if (in_string) {
                if (escape) {
                    escape = false;
                } else if (ch == '\\') {
                    escape = true;
                } else if (ch == '"') {
                    in_string = false;
                }
            } else if (ch == '"') {
                in_string = true;
            } else if (ch == '{' || ch == '[') {
                ++depth;
            } else if (ch == '}' || ch == ']') {
                if (depth == 0) {
                    /* 父结构结束 —— 表明当前不是我们要找的键，回退到 call 层 */
                    return NULL;
                }
                --depth;
                if (depth == 0) {
                    ++value_cursor;
                    break;
                }
            } else if ((depth == 0) && (ch == ',')) {
                ++value_cursor;
                break;
            }
            ++value_cursor;
        }
        cursor = skip_whitespace(value_cursor, end);
    }
    return NULL;
}

/* cursor 指向 value 起点，返回值类型，并把内容起始点/长度告诉调用方。 */
static json_value_kind_t classify_value(const uint8_t *cursor,
                                        const uint8_t *end,
                                        const uint8_t **value_start,
                                        const uint8_t **value_end)
{
    cursor = skip_whitespace(cursor, end);
    if (cursor >= end) {
        return JSON_VALUE_NULL;
    }
    switch (*cursor) {
    case '"':
        /* 字符串：找到下一个不带前导反斜杠的引号 */
        *value_start = cursor + 1U;
        ++cursor;
        while (cursor < end) {
            if (*cursor == '\\') {
                if (cursor + 1U >= end) {
                    return JSON_VALUE_NULL;
                }
                cursor += 2U;
                continue;
            }
            if (*cursor == '"') {
                *value_end = cursor;
                return JSON_VALUE_STRING;
            }
            ++cursor;
        }
        return JSON_VALUE_NULL;
    case 't':
    case 'f':
        *value_start = cursor;
        *value_end = cursor + (*cursor == 't' ? 4U : 5U);
        return JSON_VALUE_BOOL;
    case 'n':
        *value_start = cursor;
        *value_end = cursor + 4U;
        return JSON_VALUE_NULL;
    default:
        /* 假定为整数 —— 本项目 MQTT payload 不发浮点。 */
        *value_start = cursor;
        while ((cursor < end) && (*cursor != ',') && (*cursor != '}') &&
               (*cursor != ' ') && (*cursor != '\n') && (*cursor != '\r') &&
               (*cursor != '\t')) {
            ++cursor;
        }
        *value_end = cursor;
        return JSON_VALUE_INT;
    }
}

/* 进入 path 的下一段（一个键），要么命中叶子，要么找到子对象。 */
static bool descend(const uint8_t *buffer, size_t length, const char *path,
                    const uint8_t **value_start, const uint8_t **value_end,
                    json_value_kind_t *kind)
{
    const uint8_t *cursor = buffer;
    const uint8_t *end = buffer + length;
    const char *segment = path;
    const char *next;
    char key[32];
    size_t key_length;

    while (*segment != '\0') {
        next = segment;
        while ((*next != '\0') && (*next != '.')) {
            ++next;
        }
        key_length = (size_t)(next - segment);
        if (key_length >= sizeof(key)) {
            return false;
        }
        memcpy(key, segment, key_length);
        key[key_length] = '\0';

        const uint8_t *value_cursor = NULL;
        cursor = find_key(cursor, end, key, &value_cursor);
        if (cursor == NULL) {
            return false;
        }
        cursor = value_cursor;
        if (*next == '\0') {
            /* 叶子 */
            cursor = skip_whitespace(cursor, end);
            json_value_kind_t leaf_kind =
                classify_value(cursor, end, value_start, value_end);
            if (kind != NULL) {
                *kind = leaf_kind;
            }
            return leaf_kind != JSON_VALUE_NULL;
        }
        /* 中间节点必须是对象，再深入下一层。 */
        cursor = skip_whitespace(cursor, end);
        if ((cursor >= end) || (*cursor != '{')) {
            /* key 找到了但下一层不是对象 —— 说明 schema 写错了 */
            return false;
        }
        segment = next + 1U;
    }
    return false;
}

bool json_reader_find_int(const uint8_t *buffer, size_t length,
                          const char *path, int64_t *out_value)
{
    const uint8_t *value_start = NULL;
    const uint8_t *value_end = NULL;
    json_value_kind_t kind = JSON_VALUE_NULL;
    int64_t result = 0;
    bool negative = false;

    if ((buffer == NULL) || (length == 0U) || (path == NULL) ||
        (out_value == NULL)) {
        return false;
    }
    if (!descend(buffer, length, path, &value_start, &value_end, &kind)) {
        return false;
    }
    if (kind != JSON_VALUE_INT) {
        return false;
    }
    for (const uint8_t *c = value_start; c < value_end; ++c) {
        if ((*c < '0') || (*c > '9')) {
            if ((*c == '-') && (c == value_start)) {
                negative = true;
                continue;
            }
            return false;
        }
        /* 简单防御：避免溢出（int64）。 */
        if (result > (int64_t)((UINT64_C(0x7FFFFFFFFFFFFFFF) - 9U) / 10U)) {
            return false;
        }
        result = (result * 10) + (int64_t)(*c - '0');
    }
    *out_value = negative ? -result : result;
    return true;
}

bool json_reader_find_bool(const uint8_t *buffer, size_t length,
                           const char *path, bool *out_value)
{
    const uint8_t *value_start = NULL;
    const uint8_t *value_end = NULL;
    json_value_kind_t kind = JSON_VALUE_NULL;

    if ((buffer == NULL) || (length == 0U) || (path == NULL) ||
        (out_value == NULL)) {
        return false;
    }
    if (!descend(buffer, length, path, &value_start, &value_end, &kind)) {
        return false;
    }
    if (kind != JSON_VALUE_BOOL) {
        return false;
    }
    *out_value = (value_end - value_start) == 4U;
    return true;
}

bool json_reader_find_string(const uint8_t *buffer, size_t length,
                             const char *path, char *out_string,
                             size_t out_string_capacity,
                             size_t *out_string_length)
{
    const uint8_t *value_start = NULL;
    const uint8_t *value_end = NULL;
    json_value_kind_t kind = JSON_VALUE_NULL;
    size_t copy;

    if ((buffer == NULL) || (length == 0U) || (path == NULL) ||
        (out_string == NULL) || (out_string_capacity == 0U)) {
        return false;
    }
    if (!descend(buffer, length, path, &value_start, &value_end, &kind)) {
        return false;
    }
    if (kind != JSON_VALUE_STRING) {
        return false;
    }
    copy = (size_t)(value_end - value_start);
    if (copy >= out_string_capacity) {
        copy = out_string_capacity - 1U;
    }
    memcpy(out_string, value_start, copy);
    out_string[copy] = '\0';
    if (out_string_length != NULL) {
        *out_string_length = copy;
    }
    return true;
}
