#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    bool valid;
} rtc_time_t;

extern rtc_time_t rtc_time;
extern SemaphoreHandle_t mutex_time;

void rtc_task(void *arg);
