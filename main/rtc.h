#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
} rtc_time_t;

bool rtc_read(rtc_time_t *t);
