#include "lock_auth.h"

bool lock_constant_time_equal(const uint8_t *left, const uint8_t *right,
                              size_t length)
{
    uint8_t difference = 0U;
    if ((left == NULL) || (right == NULL)) {
        return false;
    }
    for (size_t i = 0U; i < length; ++i) {
        difference |= (uint8_t)(left[i] ^ right[i]);
    }
    return difference == 0U;
}

static bool pin_matches(const char *stored_pin, size_t stored_length,
                        const char *input, size_t input_length,
                        bool allow_virtual)
{
    size_t last_start;

    if ((stored_length == 0U) || (input_length < stored_length)) {
        return false;
    }
    if ((!allow_virtual) && (input_length != stored_length)) {
        return false;
    }
    last_start = allow_virtual ? (input_length - stored_length) : 0U;
    uint8_t matched = 0U;
    for (size_t start = 0U; start <= last_start; ++start) {
        uint8_t difference = 0U;
        for (size_t i = 0U; i < stored_length; ++i) {
            difference |= (uint8_t)stored_pin[i] ^ (uint8_t)input[start + i];
        }
        if (difference == 0U) {
            matched = 1U;
        }
    }
    return matched != 0U;
}

lock_auth_result_t lock_auth_verify_pin(
    lock_auth_state_t *state,
    const lock_auth_policy_t *policy,
    const char *stored_pin,
    size_t stored_pin_length,
    const char *input,
    size_t input_length,
    bool allow_virtual_prefix_suffix,
    uint64_t now)
{
    if ((state == NULL) || (policy == NULL) || (stored_pin == NULL) ||
        (input == NULL) || (policy->maximum_failures == 0U) ||
        (stored_pin_length < LOCK_PIN_MIN_LENGTH) ||
        (stored_pin_length > LOCK_PIN_MAX_LENGTH) ||
        (input_length > LOCK_INPUT_MAX_LENGTH)) {
        return LOCK_AUTH_INVALID_ARGUMENT;
    }

    if (now < state->locked_until) {
        return LOCK_AUTH_LOCKED;
    }
    if (state->locked_until != 0U) {
        state->locked_until = 0U;
        state->failed_attempts = 0U;
    }

    if (pin_matches(stored_pin, stored_pin_length, input, input_length,
                    allow_virtual_prefix_suffix)) {
        state->failed_attempts = 0U;
        return LOCK_AUTH_GRANTED;
    }

    if (state->failed_attempts < UINT8_MAX) {
        ++state->failed_attempts;
    }
    if (state->failed_attempts >= policy->maximum_failures) {
        state->locked_until = now + policy->lockout_seconds;
        return LOCK_AUTH_LOCKED;
    }
    return LOCK_AUTH_DENIED;
}
