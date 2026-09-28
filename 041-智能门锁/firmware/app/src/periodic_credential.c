#include "periodic_credential.h"

#include <limits.h>
#include <string.h>

bool periodic_credential_set(periodic_credential_t *credential,
                             const uint8_t *device_secret,
                             size_t device_secret_length,
                             const char *pin,
                             size_t pin_length,
                             uint8_t days_mask,
                             uint16_t start_minute,
                             uint16_t end_minute,
                             int16_t utc_offset_minutes)
{
    if ((credential == NULL) || (days_mask == 0U) ||
        ((days_mask & UINT8_C(0x80)) != 0U) ||
        (start_minute >= 1440U) || (end_minute > 1440U) ||
        (start_minute >= end_minute) || (utc_offset_minutes < -720) ||
        (utc_offset_minutes > 840)) {
        return false;
    }
    memset(credential, 0, sizeof(*credential));
    if (!pin_credential_set(&credential->pin, device_secret,
                            device_secret_length, pin, pin_length)) {
        return false;
    }
    credential->days_mask = days_mask;
    credential->start_minute = start_minute;
    credential->end_minute = end_minute;
    credential->utc_offset_minutes = utc_offset_minutes;
    credential->enabled = true;
    return true;
}

bool periodic_credential_verify(const periodic_credential_t *credential,
                                const uint8_t *device_secret,
                                size_t device_secret_length,
                                const char *input,
                                size_t input_length,
                                bool allow_virtual,
                                uint64_t unix_time)
{
    int64_t local_seconds;
    uint64_t local_days;
    uint16_t minute;
    uint8_t weekday;

    if ((credential == NULL) || (!credential->enabled) ||
        (unix_time > (uint64_t)INT64_MAX)) {
        return false;
    }
    local_seconds = (int64_t)unix_time +
                    ((int64_t)credential->utc_offset_minutes * INT64_C(60));
    if (local_seconds < 0) {
        return false;
    }
    local_days = (uint64_t)local_seconds / UINT64_C(86400);
    minute = (uint16_t)(((uint64_t)local_seconds % UINT64_C(86400)) /
                        UINT64_C(60));
    weekday = (uint8_t)((local_days + UINT64_C(3)) % UINT64_C(7));
    if (((credential->days_mask & (uint8_t)(UINT8_C(1) << weekday)) == 0U) ||
        (minute < credential->start_minute) ||
        (minute >= credential->end_minute)) {
        return false;
    }
    return pin_credential_matches(&credential->pin, device_secret,
                                  device_secret_length, input, input_length,
                                  allow_virtual);
}
