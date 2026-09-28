#ifndef SMART_LOCK_VISITOR_CREDENTIAL_H
#define SMART_LOCK_VISITOR_CREDENTIAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sha1.h"

typedef struct {
    uint8_t code_tag[SHA1_DIGEST_SIZE];
    uint64_t valid_from;
    uint64_t valid_until;
    uint8_t uses_remaining;
    bool active;
} visitor_credential_t;

bool visitor_credential_issue(visitor_credential_t *credential,
                              const uint8_t *device_secret,
                              size_t device_secret_length,
                              const char *code, size_t code_length,
                              uint64_t valid_from, uint64_t valid_until,
                              uint8_t permitted_uses);

bool visitor_credential_verify_and_consume(visitor_credential_t *credential,
                                           const uint8_t *device_secret,
                                           size_t device_secret_length,
                                           const char *candidate,
                                           size_t candidate_length,
                                           uint64_t now);

#endif
