#define LCD_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <stdbool.h>

/* ================= CONFIG ================= */
#define PCF8574_ADDR 0x27
#define I2C_MASTER_NUM        I2C_NUM_0

/* ================= INTERNAL ================= */
static void lcd_delay(void) { vTaskDelay(pdMS_TO_TICKS(5)); }

static void pcf8574_write(uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (PCF8574_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
}

static void lcd_send_nibble(uint8_t n, bool rs)
{
    uint8_t d = (n << 4) | (1 << 3);
    if (rs) d |= 1;
    pcf8574_write(d | (1 << 2));
    lcd_delay();
    pcf8574_write(d);
    lcd_delay();
}

static void lcd_send_byte(uint8_t b, bool rs)
{
    lcd_send_nibble(b >> 4, rs);
    lcd_send_nibble(b & 0x0F, rs);
}

static void lcd_init(void)
{
    lcd_delay();
    lcd_send_nibble(0x03, false);
    lcd_send_nibble(0x03, false);
    lcd_send_nibble(0x03, false);
    lcd_send_nibble(0x02, false);
    lcd_send_byte(0x28, false);
    lcd_send_byte(0x0C, false);
    lcd_send_byte(0x01, false);
    lcd_send_byte(0x06, false);
}

static void lcd_cmd(uint8_t c) { lcd_send_byte(c, false); }
static void lcd_data(uint8_t d) { lcd_send_byte(d, true); }

