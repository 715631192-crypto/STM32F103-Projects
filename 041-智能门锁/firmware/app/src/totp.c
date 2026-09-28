#include "totp.h"

#include "sha1.h"

static uint32_t decimal_modulus(uint8_t digits)
{
    uint32_t modulus = 1U;
    for (uint8_t i = 0U; i < digits; ++i) {
        modulus *= 10U;
    }
    return modulus;
}

static uint32_t hotp_generate(const uint8_t *secret, size_t secret_length,
                              uint64_t counter, uint8_t digits)
{
    uint8_t counter_bytes[8];
    uint8_t digest[SHA1_DIGEST_SIZE];
    uint32_t binary;
    uint8_t offset;

    for (size_t i = 0U; i < sizeof(counter_bytes); ++i) {
        counter_bytes[7U - i] = (uint8_t)(counter >> (i * 8U));
    }

    hmac_sha1(secret, secret_length, counter_bytes, sizeof(counter_bytes), digest);
    offset = (uint8_t)(digest[SHA1_DIGEST_SIZE - 1U] & UINT8_C(0x0F));
    binary = ((uint32_t)(digest[offset] & UINT8_C(0x7F)) << 24U) |
             ((uint32_t)digest[offset + 1U] << 16U) |
             ((uint32_t)digest[offset + 2U] << 8U) |
             (uint32_t)digest[offset + 3U];
    return binary % decimal_modulus(digits);
}

uint32_t totp_generate(const uint8_t *secret, size_t secret_length,
                       uint64_t unix_time, uint32_t time_step,
                       uint8_t digits)
{
    if ((secret == NULL) || (secret_length == 0U) || (time_step == 0U) ||
        (digits < 6U) || (digits > 8U)) {
        return UINT32_MAX;
    }
    return hotp_generate(secret, secret_length, unix_time / time_step, digits);
}

bool totp_verify(const uint8_t *secret, size_t secret_length,
                 uint64_t unix_time, uint32_t time_step, uint8_t digits,
                 uint32_t candidate, uint8_t allowed_window)
{
    const uint64_t base_counter = (time_step == 0U) ? 0U : unix_time / time_step;

    if ((secret == NULL) || (secret_length == 0U) || (time_step == 0U) ||
        (digits < 6U) || (digits > 8U) || (allowed_window > 2U)) {
        return false;
    }

    for (int32_t delta = -(int32_t)allowed_window;
         delta <= (int32_t)allowed_window; ++delta) {
        uint64_t counter;
        if ((delta < 0) && (base_counter < (uint64_t)(-delta))) {
            continue;
        }
        counter = (delta < 0) ? base_counter - (uint64_t)(-delta)
                              : base_counter + (uint64_t)delta;
        if (hotp_generate(secret, secret_length, counter, digits) == candidate) {
            return true;
        }
    }
    return false;
}
