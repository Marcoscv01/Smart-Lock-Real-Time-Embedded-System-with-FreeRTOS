#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

void teclado_init(void);
void Teclado_Scan_task(void *pvParameters);
void teclado_event_task(void *arg);