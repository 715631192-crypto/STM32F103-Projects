#ifndef SMART_LOCK_TOTP_H
#define SMART_LOCK_TOTP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint32_t totp_generate(const uint8_t *secret, size_t secret_length,
                       uint64_t unix_time, uint32_t time_step,
                       uint8_t digits);

bool totp_verify(const uint8_t *secret, size_t secret_length,
                 uint64_t unix_time, uint32_t time_step, uint8_t digits,
                 uint32_t candidate, uint8_t allowed_window);

#endif
