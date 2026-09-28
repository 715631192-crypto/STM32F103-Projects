#ifndef SMART_LOCK_PERIODIC_CREDENTIAL_H
#define SMART_LOCK_PERIODIC_CREDENTIAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pin_credential.h"

/* 位 0 表示周一，位 6 表示周日。时间为本地时间零点之后的分钟数。 */
typedef struct {
    pin_credential_t pin;
    uint8_t days_mask;
    uint16_t start_minute;
    uint16_t end_minute;
    int16_t utc_offset_minutes;
    bool enabled;
} periodic_credential_t;

bool periodic_credential_set(periodic_credential_t *credential,
                             const uint8_t *device_secret,
                             size_t device_secret_length,
                             const char *pin,
                             size_t pin_length,
                             uint8_t days_mask,
                             uint16_t start_minute,
                             uint16_t end_minute,
                             int16_t utc_offset_minutes);

bool periodic_credential_verify(const periodic_credential_t *credential,
                                const uint8_t *device_secret,
                                size_t device_secret_length,
                                const char *input,
                                size_t input_length,
                                bool allow_virtual,
                                uint64_t unix_time);

#endif
