#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "servo.h"
#include "fsm.h"
#include "freertos/queue.h"
#include "esp_log.h"

#define SERVO_GPIO  GPIO_NUM_15

extern QueueHandle_t q_fsm_servo;

void servo_init(void) {
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_16_BIT,
        .freq_hz = 50
    };
    ledc_timer_config(&t);

    ledc_channel_config_t c = {
        .gpio_num = SERVO_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0
    };
    ledc_channel_config(&c);
}

static void servo_set_us(uint32_t us) {
    uint32_t duty = (us * 65535) / 20000;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void servo_task(void *arg)
{
    fsm_display_t msg;
    fsm_state_t last_state = -1;

    for (;;) {
        xQueueReceive(q_fsm_servo, &msg, portMAX_DELAY);
        int64_t now = esp_timer_get_time();
        ESP_LOGI("LAT_SERVO", "FSM→SERVO = %lld us", now - msg.timestamp_us);

        if (msg.state == last_state)
            continue;

        last_state = msg.state;

        switch (msg.state) {

            case ST_ABIERTO:
                servo_set_us(SERVO_OPEN_US);
                break;
            case ST_PANIC:
                servo_set_us(SERVO_OPEN_US);
                break;

            case ST_IDLE:
            case ST_ERROR:
                servo_set_us(SERVO_CLOSE_US);
                break;

            default: servo_set_us(SERVO_CLOSE_US);
                break;
        }
    }
}
