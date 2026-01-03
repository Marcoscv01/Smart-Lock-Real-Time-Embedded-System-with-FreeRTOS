#include "lcd.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include <string.h>
#include <stdio.h>
#include "rtc.h"

#define I2C_PORT     I2C_NUM_0
#define PCF8574_ADDR 0x27

static SemaphoreHandle_t mutex_lcd;
static char lcd_line1[17];
static char lcd_line2[21];

/* ===== LOW LEVEL ===== */
static void lcd_delay(void) { vTaskDelay(pdMS_TO_TICKS(5)); }

static void pcf_write(uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (PCF8574_ADDR << 1), true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
}

static void lcd_send_nibble(uint8_t nibble, bool rs) {
    uint8_t d = nibble << 4;
    if (rs) d |= 0x01;
    d |= 0x08;
    pcf_write(d | 0x04); 
    lcd_delay();
    pcf_write(d & ~0x04); 
    lcd_delay();
}

static void lcd_cmd(uint8_t cmd) {
    lcd_send_nibble(cmd >> 4, false);
    lcd_send_nibble(cmd & 0x0F, false);
}

static void lcd_data(uint8_t data) {
    lcd_send_nibble(data >> 4, true);
    lcd_send_nibble(data & 0x0F, true);
}

static void lcd_init_hw(void) {
    lcd_delay();
    lcd_send_nibble(0x03, false);
    lcd_send_nibble(0x03, false);
    lcd_send_nibble(0x03, false);
    lcd_send_nibble(0x02, false);

    lcd_cmd(0x28); // 4-bit, 2 líneas
    lcd_cmd(0x0C); // display on, cursor off
    lcd_cmd(0x01); // clear display
    lcd_cmd(0x06); // entry mode
}

/* ===== API ===== */
void lcd_set_fsm_state(int state, const char *pin) {
    xSemaphoreTake(mutex_lcd, portMAX_DELAY);
    switch(state) {
        case 0: snprintf(lcd_line1, sizeof(lcd_line1), "Inicializando"); break;
        case 1: snprintf(lcd_line1, sizeof(lcd_line1), "Esperando PIN"); break;
        case 2: snprintf(lcd_line1, sizeof(lcd_line1), "PIN: %-11s", pin ? pin : ""); break;
        case 3: snprintf(lcd_line1, sizeof(lcd_line1), "Abriendo..."); break;
        case 4: snprintf(lcd_line1, sizeof(lcd_line1), "!!!PANIC!!!"); break;
        case 5: snprintf(lcd_line1, sizeof(lcd_line1), "ERROR     "); break;
        default: snprintf(lcd_line1, sizeof(lcd_line1), "???"); break;
    }
    xSemaphoreGive(mutex_lcd);
}

/* ===== TAREA LCD ===== */
void lcd_task(void *arg) {
    mutex_lcd = xSemaphoreCreateMutex();
    lcd_init_hw();

    for (;;) {
        rtc_time_t t;
        if (rtc_read(&t)) {
            // Mostrar fecha y hora dd/mm hh:mm:ss
            snprintf(lcd_line2, sizeof(lcd_line2),
                     "%02d/%02d %02d:%02d:%02d ",
                     t.day, t.month,t.hour, t.min, t.sec);
        } else {
            snprintf(lcd_line2, sizeof(lcd_line2), "RTC ERROR      ");
        }

        xSemaphoreTake(mutex_lcd, portMAX_DELAY);
        lcd_cmd(0x80); for(int i=0;i<16;i++) lcd_data(lcd_line1[i]);
        lcd_cmd(0xC0); for(int i=0;i<16;i++) lcd_data(lcd_line2[i]);
        xSemaphoreGive(mutex_lcd);

        vTaskDelay(pdMS_TO_TICKS(300));
    }
}
