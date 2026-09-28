#ifndef SMART_LOCK_DS3231_H
#define SMART_LOCK_DS3231_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef bool (*ds3231_read_fn)(void *context, uint8_t reg,
                               uint8_t *data, size_t length);
typedef bool (*ds3231_write_fn)(void *context, uint8_t reg,
                                const uint8_t *data, size_t length);

typedef struct {
    void *context;
    ds3231_read_fn read;
    ds3231_write_fn write;
} ds3231_t;

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} ds3231_datetime_t;

bool ds3231_read_datetime(const ds3231_t *device,
                          ds3231_datetime_t *datetime,
                          bool *time_trusted);
bool ds3231_datetime_to_unix(const ds3231_datetime_t *datetime,
                             uint64_t *unix_time);
bool ds3231_read_unix(const ds3231_t *device, uint64_t *unix_time);
bool ds3231_set_datetime(const ds3231_t *device,
                         const ds3231_datetime_t *datetime);

#endif
