#include "pin_credential.h"

#include <string.h>

static bool all_digits(const char *value, size_t length)
{
    for (size_t i = 0U; i < length; ++i) {
        if ((value[i] < '0') || (value[i] > '9')) {
            return false;
        }
    }
    return true;
}

bool pin_credential_set(pin_credential_t *credential,
                        const uint8_t *device_secret,
                        size_t device_secret_length,
                        const char *pin,
                        size_t pin_length)
{
    if ((credential == NULL) || (device_secret == NULL) || (pin == NULL) ||
        (device_secret_length < 16U) || (pin_length < LOCK_PIN_MIN_LENGTH) ||
        (pin_length > LOCK_PIN_MAX_LENGTH) || (!all_digits(pin, pin_length))) {
        return false;
    }
    memset(credential, 0, sizeof(*credential));
    hmac_sha1(device_secret, device_secret_length, (const uint8_t *)pin,
              pin_length, credential->tag);
    credential->pin_length = (uint8_t)pin_length;
    credential->valid = true;
    return true;
}

bool pin_credential_matches(const pin_credential_t *credential,
                            const uint8_t *secret, size_t secret_length,
                            const char *input, size_t input_length,
                            bool allow_virtual)
{
    uint8_t candidate_tag[SHA1_DIGEST_SIZE];
    uint8_t any_match = 0U;
    size_t pin_length;
    size_t last_start;

    if ((credential == NULL) || (!credential->valid) || (secret == NULL) ||
        (secret_length < 16U) || (input == NULL) ||
        (input_length > LOCK_INPUT_MAX_LENGTH)) {
        return false;
    }
    pin_length = credential->pin_length;
    if ((pin_length < LOCK_PIN_MIN_LENGTH) ||
        (pin_length > LOCK_PIN_MAX_LENGTH) || (input_length < pin_length)) {
        return false;
    }
    if ((!allow_virtual) && (input_length != pin_length)) {
        return false;
    }
    last_start = allow_virtual ? (input_length - pin_length) : 0U;

    for (size_t start = 0U; start <= last_start; ++start) {
        hmac_sha1(secret, secret_length, (const uint8_t *)&input[start],
                  pin_length, candidate_tag);
        if (lock_constant_time_equal(candidate_tag, credential->tag,
                                     sizeof(candidate_tag))) {
            any_match = 1U;
        }
    }
    memset(candidate_tag, 0, sizeof(candidate_tag));
    return any_match != 0U;
}

lock_auth_result_t pin_credential_verify(
    const pin_credential_t *credential,
    const uint8_t *device_secret,
    size_t device_secret_length,
    const char *input,
    size_t input_length,
    bool allow_virtual_prefix_suffix,
    uint64_t now,
    const lock_auth_policy_t *policy,
    lock_auth_state_t *state)
{
    if ((credential == NULL) || (!credential->valid) ||
        (device_secret == NULL) || (device_secret_length < 16U) ||
        (input == NULL) || (input_length > LOCK_INPUT_MAX_LENGTH) ||
        (policy == NULL) || (state == NULL) ||
        (policy->maximum_failures == 0U)) {
        return LOCK_AUTH_INVALID_ARGUMENT;
    }
    if (now < state->locked_until) {
        return LOCK_AUTH_LOCKED;
    }
    if (state->locked_until != 0U) {
        state->locked_until = 0U;
        state->failed_attempts = 0U;
    }

    if (pin_credential_matches(credential, device_secret, device_secret_length,
                               input, input_length,
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
