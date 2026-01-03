#ifndef LCD_H
#define LCD_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

void lcd_task(void *arg);
void lcd_set_fsm_state(int state, const char *pin);

#endif
