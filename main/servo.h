#pragma once
#include "freertos/FreeRTOS.h"

#define SERVO_OPEN_US    2000
#define SERVO_CLOSE_US   1000
#define SERVO_TIME_MS    3000

void servo_init(void);
void servo_set_us(uint32_t us);