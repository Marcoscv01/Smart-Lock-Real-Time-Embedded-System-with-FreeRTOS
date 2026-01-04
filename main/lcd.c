#include "lcd.h"
#include "rtc.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include <stdio.h>
#include <string.h>

#define I2C_PORT     I2C_NUM_0
#define PCF8574_ADDR 0x27

static SemaphoreHandle_t mutex_lcd;
static char lcd_line1[17] = "Inicializando";
static char lcd_line2[17] = "";

/* ===== HW LCD ===== */
static void lcd_delay(void){ vTaskDelay(pdMS_TO_TICKS(5)); }

static void pcf_write(uint8_t d){
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, PCF8574_ADDR<<1, true);
    i2c_master_write_byte(cmd, d, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
}

static void lcd_nibble(uint8_t n, bool rs){
    uint8_t d = (n<<4) | 0x08 | (rs?1:0);
    pcf_write(d | 0x04); lcd_delay();
    pcf_write(d & ~0x04); lcd_delay();
}

static void lcd_cmd(uint8_t c){
    lcd_nibble(c>>4,false);
    lcd_nibble(c&0x0F,false);
}

static void lcd_data(uint8_t d){
    lcd_nibble(d>>4,true);
    lcd_nibble(d&0x0F,true);
}

static void lcd_hw_init(void){
    lcd_delay();
    lcd_nibble(0x03,false);
    lcd_nibble(0x03,false);
    lcd_nibble(0x03,false);
    lcd_nibble(0x02,false);
    lcd_cmd(0x28);
    lcd_cmd(0x0C);
    lcd_cmd(0x01);
    lcd_cmd(0x06);
}

/* ===== API ===== */
void lcd_set_fsm_state(int state, const char *pin)
{
    xSemaphoreTake(mutex_lcd, portMAX_DELAY);

    switch(state){
        case 0: snprintf(lcd_line1,17,"Inicializando"); break;
        case 1: snprintf(lcd_line1,17,"Esperando PIN"); break;
        case 2: snprintf(lcd_line1,17,"PIN: %-11s", pin); break;
        case 3: snprintf(lcd_line1,17,"Abriendo..."); break;
        case 4: snprintf(lcd_line1,17,"!!! PANIC !!!"); break;
        case 5: snprintf(lcd_line1,17,"ERROR"); break;
    }

    xSemaphoreGive(mutex_lcd);
}

void lcd_task(void *arg)
{
    mutex_lcd = xSemaphoreCreateMutex();
    lcd_hw_init();

    for (;;)
    {
        char l2[17];

        /* ===== LEER RECURSO TIEMPO ===== */
        xSemaphoreTake(mutex_time, portMAX_DELAY);

        if (rtc_time.valid) {
            snprintf(l2, 21,
                     "%02d/%02d %02d:%02d:%02d",
                     rtc_time.day,
                     rtc_time.month,
                     rtc_time.hour,
                     rtc_time.min,
                     rtc_time.sec);
        } else {
            snprintf(l2, sizeof(l2), "RTC NO VALIDO ");
        }

        xSemaphoreGive(mutex_time);

        /* ===== ESCRITURA LCD ===== */
        xSemaphoreTake(mutex_lcd, portMAX_DELAY);

        lcd_cmd(0x80);
        for (int i = 0; i < 16; i++) lcd_data(lcd_line1[i]);

        lcd_cmd(0xC0);
        for (int i = 0; i < 16; i++) lcd_data(l2[i]);

        xSemaphoreGive(mutex_lcd);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
