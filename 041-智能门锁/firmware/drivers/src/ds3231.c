#include "ds3231.h"

#define DS3231_REG_TIME 0x00U
#define DS3231_REG_STATUS 0x0FU
#define DS3231_STATUS_OSF 0x80U

static uint8_t from_bcd(uint8_t value)
{
    return (uint8_t)(((value >> 4U) * 10U) + (value & 0x0FU));
}

static uint8_t to_bcd(uint8_t value)
{
    return (uint8_t)(((value / 10U) << 4U) | (value % 10U));
}

static bool leap_year(uint16_t year)
{
    return ((year % 4U) == 0U) &&
           (((year % 100U) != 0U) || ((year % 400U) == 0U));
}

static uint8_t days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {
        31U, 28U, 31U, 30U, 31U, 30U,
        31U, 31U, 30U, 31U, 30U, 31U
    };
    if ((month == 0U) || (month > 12U)) {
        return 0U;
    }
    if ((month == 2U) && leap_year(year)) {
        return 29U;
    }
    return days[month - 1U];
}

static bool datetime_valid(const ds3231_datetime_t *value)
{
    return (value != NULL) && (value->year >= 2000U) &&
           (value->year <= 2099U) && (value->month >= 1U) &&
           (value->month <= 12U) && (value->day >= 1U) &&
           (value->day <= days_in_month(value->year, value->month)) &&
           (value->hour < 24U) && (value->minute < 60U) &&
           (value->second < 60U);
}

bool ds3231_read_datetime(const ds3231_t *device,
                          ds3231_datetime_t *datetime,
                          bool *time_trusted)
{
    uint8_t registers[7];
    uint8_t status;
    uint8_t hour;

    if ((device == NULL) || (device->read == NULL) || (datetime == NULL) ||
        (time_trusted == NULL) ||
        (!device->read(device->context, DS3231_REG_TIME, registers,
                       sizeof(registers))) ||
        (!device->read(device->context, DS3231_REG_STATUS, &status, 1U))) {
        return false;
    }
    datetime->second = from_bcd((uint8_t)(registers[0] & 0x7FU));
    datetime->minute = from_bcd((uint8_t)(registers[1] & 0x7FU));
    hour = registers[2];
    if ((hour & 0x40U) != 0U) {
        const bool afternoon = (hour & 0x20U) != 0U;
        uint8_t hour12 = from_bcd((uint8_t)(hour & 0x1FU));
        if ((hour12 == 0U) || (hour12 > 12U)) {
            return false;
        }
        hour12 = (uint8_t)(hour12 % 12U);
        datetime->hour = (uint8_t)(hour12 + (afternoon ? 12U : 0U));
    } else {
        datetime->hour = from_bcd((uint8_t)(hour & 0x3FU));
    }
    datetime->day = from_bcd((uint8_t)(registers[4] & 0x3FU));
    datetime->month = from_bcd((uint8_t)(registers[5] & 0x1FU));
    datetime->year = (uint16_t)(2000U + from_bcd(registers[6]));
    *time_trusted = (status & DS3231_STATUS_OSF) == 0U;
    return datetime_valid(datetime);
}

bool ds3231_datetime_to_unix(const ds3231_datetime_t *datetime,
                             uint64_t *unix_time)
{
    uint64_t days = 0U;
    if ((!datetime_valid(datetime)) || (unix_time == NULL)) {
        return false;
    }
    for (uint16_t year = 1970U; year < datetime->year; ++year) {
        days += leap_year(year) ? 366U : 365U;
    }
    for (uint8_t month = 1U; month < datetime->month; ++month) {
        days += days_in_month(datetime->year, month);
    }
    days += (uint64_t)(datetime->day - 1U);
    *unix_time = (days * UINT64_C(86400)) +
                 ((uint64_t)datetime->hour * UINT64_C(3600)) +
                 ((uint64_t)datetime->minute * UINT64_C(60)) +
                 datetime->second;
    return true;
}

bool ds3231_read_unix(const ds3231_t *device, uint64_t *unix_time)
{
    ds3231_datetime_t datetime;
    bool trusted;
    return ds3231_read_datetime(device, &datetime, &trusted) && trusted &&
           ds3231_datetime_to_unix(&datetime, unix_time);
}

bool ds3231_set_datetime(const ds3231_t *device,
                         const ds3231_datetime_t *datetime)
{
    uint8_t registers[7];
    uint8_t status;
    if ((device == NULL) || (device->read == NULL) ||
        (device->write == NULL) || (!datetime_valid(datetime))) {
        return false;
    }
    registers[0] = to_bcd(datetime->second);
    registers[1] = to_bcd(datetime->minute);
    registers[2] = to_bcd(datetime->hour);
    registers[3] = 1U;
    registers[4] = to_bcd(datetime->day);
    registers[5] = to_bcd(datetime->month);
    registers[6] = to_bcd((uint8_t)(datetime->year - 2000U));
    if ((!device->write(device->context, DS3231_REG_TIME, registers,
                        sizeof(registers))) ||
        (!device->read(device->context, DS3231_REG_STATUS, &status, 1U))) {
        return false;
    }
    status &= (uint8_t)~DS3231_STATUS_OSF;
    return device->write(device->context, DS3231_REG_STATUS, &status, 1U);
}
