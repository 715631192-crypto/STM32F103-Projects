#include "sha1.h"

#include <string.h>

static uint32_t rotate_left(uint32_t value, uint8_t bits)
{
    return (value << bits) | (value >> (32U - bits));
}

static uint32_t read_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24U) |
           ((uint32_t)data[1] << 16U) |
           ((uint32_t)data[2] << 8U) |
           (uint32_t)data[3];
}

static void write_be32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24U);
    data[1] = (uint8_t)(value >> 16U);
    data[2] = (uint8_t)(value >> 8U);
    data[3] = (uint8_t)value;
}

static void sha1_transform(sha1_context_t *context, const uint8_t block[64])
{
    uint32_t words[80];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;

    for (size_t i = 0U; i < 16U; ++i) {
        words[i] = read_be32(&block[i * 4U]);
    }
    for (size_t i = 16U; i < 80U; ++i) {
        words[i] = rotate_left(words[i - 3U] ^ words[i - 8U] ^
                               words[i - 14U] ^ words[i - 16U], 1U);
    }

    a = context->state[0];
    b = context->state[1];
    c = context->state[2];
    d = context->state[3];
    e = context->state[4];

    for (size_t i = 0U; i < 80U; ++i) {
        uint32_t function;
        uint32_t constant;
        uint32_t temporary;

        if (i < 20U) {
            function = (b & c) | ((~b) & d);
            constant = UINT32_C(0x5A827999);
        } else if (i < 40U) {
            function = b ^ c ^ d;
            constant = UINT32_C(0x6ED9EBA1);
        } else if (i < 60U) {
            function = (b & c) | (b & d) | (c & d);
            constant = UINT32_C(0x8F1BBCDC);
        } else {
            function = b ^ c ^ d;
            constant = UINT32_C(0xCA62C1D6);
        }

        temporary = rotate_left(a, 5U) + function + e + constant + words[i];
        e = d;
        d = c;
        c = rotate_left(b, 30U);
        b = a;
        a = temporary;
    }

    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
    context->state[4] += e;
}

void sha1_init(sha1_context_t *context)
{
    context->state[0] = UINT32_C(0x67452301);
    context->state[1] = UINT32_C(0xEFCDAB89);
    context->state[2] = UINT32_C(0x98BADCFE);
    context->state[3] = UINT32_C(0x10325476);
    context->state[4] = UINT32_C(0xC3D2E1F0);
    context->bit_count = 0U;
    context->buffer_length = 0U;
    memset(context->buffer, 0, sizeof(context->buffer));
}

void sha1_update(sha1_context_t *context, const uint8_t *data, size_t length)
{
    if ((data == NULL) || (length == 0U)) {
        return;
    }

    context->bit_count += (uint64_t)length * UINT64_C(8);

    while (length > 0U) {
        const size_t available = SHA1_BLOCK_SIZE - context->buffer_length;
        const size_t copy_length = (length < available) ? length : available;
        memcpy(&context->buffer[context->buffer_length], data, copy_length);
        context->buffer_length += copy_length;
        data += copy_length;
        length -= copy_length;

        if (context->buffer_length == SHA1_BLOCK_SIZE) {
            sha1_transform(context, context->buffer);
            context->buffer_length = 0U;
        }
    }
}

void sha1_final(sha1_context_t *context, uint8_t digest[SHA1_DIGEST_SIZE])
{
    const uint64_t original_bit_count = context->bit_count;
    uint8_t padding[SHA1_BLOCK_SIZE] = {0x80U};
    uint8_t length_bytes[8];
    const size_t padding_length = (context->buffer_length < 56U)
                                      ? (56U - context->buffer_length)
                                      : (120U - context->buffer_length);

    for (size_t i = 0U; i < sizeof(length_bytes); ++i) {
        length_bytes[7U - i] = (uint8_t)(original_bit_count >> (i * 8U));
    }

    sha1_update(context, padding, padding_length);
    sha1_update(context, length_bytes, sizeof(length_bytes));

    for (size_t i = 0U; i < 5U; ++i) {
        write_be32(&digest[i * 4U], context->state[i]);
    }

    memset(context, 0, sizeof(*context));
}

void sha1_digest(const uint8_t *data, size_t length,
                 uint8_t digest[SHA1_DIGEST_SIZE])
{
    sha1_context_t context;
    sha1_init(&context);
    sha1_update(&context, data, length);
    sha1_final(&context, digest);
}

void hmac_sha1(const uint8_t *key, size_t key_length,
               const uint8_t *message, size_t message_length,
               uint8_t digest[SHA1_DIGEST_SIZE])
{
    uint8_t normalized_key[SHA1_BLOCK_SIZE] = {0};
    uint8_t inner_pad[SHA1_BLOCK_SIZE];
    uint8_t outer_pad[SHA1_BLOCK_SIZE];
    uint8_t inner_digest[SHA1_DIGEST_SIZE];
    sha1_context_t context;

    if (key_length > SHA1_BLOCK_SIZE) {
        sha1_digest(key, key_length, normalized_key);
    } else if ((key != NULL) && (key_length > 0U)) {
        memcpy(normalized_key, key, key_length);
    }

    for (size_t i = 0U; i < SHA1_BLOCK_SIZE; ++i) {
        inner_pad[i] = (uint8_t)(normalized_key[i] ^ UINT8_C(0x36));
        outer_pad[i] = (uint8_t)(normalized_key[i] ^ UINT8_C(0x5C));
    }

    sha1_init(&context);
    sha1_update(&context, inner_pad, sizeof(inner_pad));
    sha1_update(&context, message, message_length);
    sha1_final(&context, inner_digest);

    sha1_init(&context);
    sha1_update(&context, outer_pad, sizeof(outer_pad));
    sha1_update(&context, inner_digest, sizeof(inner_digest));
    sha1_final(&context, digest);

    memset(normalized_key, 0, sizeof(normalized_key));
    memset(inner_pad, 0, sizeof(inner_pad));
    memset(outer_pad, 0, sizeof(outer_pad));
    memset(inner_digest, 0, sizeof(inner_digest));
}
