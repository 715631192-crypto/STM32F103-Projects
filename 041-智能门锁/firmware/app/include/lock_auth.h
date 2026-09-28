#ifndef SMART_LOCK_AUTH_H
#define SMART_LOCK_AUTH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LOCK_PIN_MIN_LENGTH 6U
#define LOCK_PIN_MAX_LENGTH 12U
#define LOCK_INPUT_MAX_LENGTH 32U

typedef struct {
    uint8_t maximum_failures;
    uint32_t lockout_seconds;
} lock_auth_policy_t;

typedef struct {
    uint8_t failed_attempts;
    uint64_t locked_until;
} lock_auth_state_t;

typedef enum {
    LOCK_AUTH_GRANTED = 0,
    LOCK_AUTH_DENIED,
    LOCK_AUTH_LOCKED,
    LOCK_AUTH_INVALID_ARGUMENT
} lock_auth_result_t;

bool lock_constant_time_equal(const uint8_t *left, const uint8_t *right,
                              size_t length);

lock_auth_result_t lock_auth_verify_pin(
    lock_auth_state_t *state,
    const lock_auth_policy_t *policy,
    const char *stored_pin,
    size_t stored_pin_length,
    const char *input,
    size_t input_length,
    bool allow_virtual_prefix_suffix,
    uint64_t now);

#endif
