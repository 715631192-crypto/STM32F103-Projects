#ifndef SMART_LOCK_SHA1_H
#define SMART_LOCK_SHA1_H

#include <stddef.h>
#include <stdint.h>

#define SHA1_DIGEST_SIZE 20U
#define SHA1_BLOCK_SIZE 64U

typedef struct {
    uint32_t state[5];
    uint64_t bit_count;
    uint8_t buffer[SHA1_BLOCK_SIZE];
    size_t buffer_length;
} sha1_context_t;

void sha1_init(sha1_context_t *context);
void sha1_update(sha1_context_t *context, const uint8_t *data, size_t length);
void sha1_final(sha1_context_t *context, uint8_t digest[SHA1_DIGEST_SIZE]);
void sha1_digest(const uint8_t *data, size_t length,
                 uint8_t digest[SHA1_DIGEST_SIZE]);
void hmac_sha1(const uint8_t *key, size_t key_length,
               const uint8_t *message, size_t message_length,
               uint8_t digest[SHA1_DIGEST_SIZE]);

#endif
