#include "rtc.h"
#include "driver/i2c.h"
#include "freertos/semphr.h"

#define DS3231_ADDR 0x68
#define I2C_PORT    I2C_NUM_0

extern SemaphoreHandle_t mutex_i2c;


static uint8_t bcd(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }

bool rtc_read(rtc_time_t *t) {
    uint8_t reg = 0x00;
    uint8_t d[5];

    xSemaphoreTake(mutex_i2c, portMAX_DELAY);

    esp_err_t ok = i2c_master_write_read_device(
        I2C_PORT, DS3231_ADDR, &reg, 1, d, 5, pdMS_TO_TICKS(100)
    );

    xSemaphoreGive(mutex_i2c);

    if (ok != ESP_OK) return false;

    t->sec   = bcd(d[0]);
    t->min   = bcd(d[1]);
    t->hour  = bcd(d[2] & 0x3F);
    t->day   = bcd(d[3]);
    t->month = bcd(d[4]);

    return true;
}
