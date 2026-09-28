#ifndef SMART_LOCK_PIN_CREDENTIAL_H
#define SMART_LOCK_PIN_CREDENTIAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lock_auth.h"
#include "sha1.h"

typedef struct {
    uint8_t tag[SHA1_DIGEST_SIZE];
    uint8_t pin_length;
    bool valid;
} pin_credential_t;

bool pin_credential_set(pin_credential_t *credential,
                        const uint8_t *device_secret,
                        size_t device_secret_length,
                        const char *pin,
                        size_t pin_length);

bool pin_credential_matches(const pin_credential_t *credential,
                            const uint8_t *device_secret,
                            size_t device_secret_length,
                            const char *input,
                            size_t input_length,
                            bool allow_virtual);

lock_auth_result_t pin_credential_verify(
    const pin_credential_t *credential,
    const uint8_t *device_secret,
    size_t device_secret_length,
    const char *input,
    size_t input_length,
    bool allow_virtual_prefix_suffix,
    uint64_t now,
    const lock_auth_policy_t *policy,
    lock_auth_state_t *state);

#endif
