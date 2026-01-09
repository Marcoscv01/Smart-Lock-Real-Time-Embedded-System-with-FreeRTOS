#include "rtc.h"
#include "driver/i2c.h"
#include "freertos/task.h"
#include "esp_log.h"

#define DS3231_ADDR 0x68
#define I2C_PORT    I2C_NUM_0

extern volatile int64_t rtc_jitter_last_us ;
extern volatile int64_t rtc_jitter_acc_us  ;
extern volatile int64_t rtc_jitter_max_us  ;
extern volatile uint32_t rtc_jitter_samples ;


extern SemaphoreHandle_t mutex_i2c;

static uint8_t bcd_to_dec(uint8_t v) {
    return (v >> 4) * 10 + (v & 0x0F);
}

void rtc_task(void *arg)
{
    uint8_t reg = 0x00;
    uint8_t d[7];

    TickType_t last_wake = xTaskGetTickCount();
    int64_t last_exec_us = esp_timer_get_time();

    const int64_t period_us = 200000; // 200 ms

    for (;;)
    {
        int64_t now_us = esp_timer_get_time();
        int64_t delta  = now_us - last_exec_us;
        int64_t jitter = llabs(delta - period_us);

        rtc_jitter_last_us = jitter;
        rtc_jitter_acc_us += jitter;
        rtc_jitter_samples++;

        if (jitter > rtc_jitter_max_us)
            rtc_jitter_max_us = jitter;

        last_exec_us = now_us;

        /* ===== RTC I2C ===== */
        xSemaphoreTake(mutex_i2c, portMAX_DELAY);

        esp_err_t ok = i2c_master_write_read_device(
            I2C_PORT, DS3231_ADDR, &reg, 1, d, sizeof(d), pdMS_TO_TICKS(10)
        );

        xSemaphoreGive(mutex_i2c);

        if (ok == ESP_OK) {
            xSemaphoreTake(mutex_time, portMAX_DELAY);

            rtc_time.sec   = bcd_to_dec(d[0]);
            rtc_time.min   = bcd_to_dec(d[1]);
            rtc_time.hour  = bcd_to_dec(d[2] & 0x3F);
            rtc_time.day   = bcd_to_dec(d[4]);
            rtc_time.month = bcd_to_dec(d[5] & 0x1F);
            rtc_time.year  = 2000 + bcd_to_dec(d[6]);
            rtc_time.valid = true;

            xSemaphoreGive(mutex_time);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(200));
    }
}


