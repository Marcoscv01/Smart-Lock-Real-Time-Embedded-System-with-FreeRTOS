#include "Teclado.h"
#include "lcd.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c.h"

#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>

/* ============================ CONFIG ============================ */

#define PIN_LENGTH       4
static const char PIN_CORRECTO[PIN_LENGTH + 1] = "1234";

/* ---- Servo ---- */
#define SERVO_GPIO       GPIO_NUM_15
#define SERVO_OPEN_US    2000
#define SERVO_CLOSE_US   1000
#define SERVO_TIME_MS    3000

/* ---- Panic ---- */
#define PANIC_GPIO       GPIO_NUM_0

/* ---- RTOS ---- */
#define FSM_QUEUE_LEN    10

#define FSM_TASK_PRIO            5
#define TECLADO_EVENT_TASK_PRIO  4
#define TECLADO_TASK_PRIO        6
#define PANIC_TASK_PRIO          7
#define LCD_TASK_PRIO            3

/* ---- I2C ---- */
#define I2C_MASTER_NUM     I2C_NUM_0
#define I2C_SDA            19
#define I2C_SCL            18
#define I2C_FREQ_HZ        100000

#define DS3231_ADDR        0x68

/* ============================ FSM ============================ */

typedef enum {
    ST_INIT = 0,
    ST_IDLE,
    ST_ENTRADA_PIN,
    ST_ABIERTO,
    ST_ERROR
} fsm_state_t;

typedef enum {
    EVT_NONE = 0,
    EVT_STARTUP,
    EVT_ENTER_IDLE,
    EVT_TECLA_DIGITO,
    EVT_TECLA_BORRAR,
    EVT_TECLA_CONFIRMAR,
    EVT_TIMEOUT,
    EVT_PANIC
} event_id_t;

typedef struct {
    event_id_t id;
    char tecla;
    int64_t timestamp_us;
} event_t;

/* ============================ RECURSOS ============================ */

QueueHandle_t cola_teclas;
static QueueHandle_t q_events;
static TimerHandle_t t_servo;
static SemaphoreHandle_t sem_panic;
static SemaphoreHandle_t mutex_i2c;
static SemaphoreHandle_t mutex_lcd;

static fsm_state_t state;

/* ============================ LCD ============================ */

static char lcd_line1[17] = "Arrancando...";
static char lcd_line2[17] = "                ";

static void lcd_set_line1(const char *txt)
{
    xSemaphoreTake(mutex_lcd, portMAX_DELAY);
    snprintf(lcd_line1, 17, "%-16s", txt);
    xSemaphoreGive(mutex_lcd);
}

/* ============================ I2C / RTC ============================ */

static uint8_t bcd_to_dec(uint8_t v)
{
    return (v >> 4) * 10 + (v & 0x0F);
}

static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

static void ds3231_read(uint8_t *h, uint8_t *m, uint8_t *s,
                        uint8_t *d, uint8_t *mo)
{
    uint8_t reg = 0x00, data[5];

    xSemaphoreTake(mutex_i2c, portMAX_DELAY);
    i2c_master_write_read_device(
        I2C_MASTER_NUM, DS3231_ADDR,
        &reg, 1, data, 5, pdMS_TO_TICKS(100)
    );
    xSemaphoreGive(mutex_i2c);

    *s  = bcd_to_dec(data[0]);
    *m  = bcd_to_dec(data[1]);
    *h  = bcd_to_dec(data[2] & 0x3F);
    *d  = bcd_to_dec(data[3]);
    *mo = bcd_to_dec(data[4]);
}

/* ============================ SERVO ============================ */

static void servo_init(void)
{
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

static void servo_set_us(uint32_t us)
{
    uint32_t duty = (us * 65535) / 20000;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

/* ============================ FSM ============================ */

static void fsm_post_event(event_t *e)
{
    e->timestamp_us = esp_timer_get_time();
    xQueueSend(q_events, e, 0);
}

static void servo_timeout_cb(TimerHandle_t xTimer)
{
    fsm_post_event(&(event_t){.id = EVT_TIMEOUT});
}

static void fsm_task(void *arg)
{
    char pin[PIN_LENGTH + 1] = {0};
    uint8_t idx = 0;

    state = ST_INIT;

    for (;;)
    {
        event_t e;
        xQueueReceive(q_events, &e, portMAX_DELAY);

        if (e.id == EVT_PANIC)
        {
            if (state != ST_ERROR)
            {
                lcd_set_line1("!!! PANIC !!!");
                servo_set_us(SERVO_OPEN_US);
                state = ST_ERROR;
            }
            else
            {
                servo_set_us(SERVO_CLOSE_US);
                state = ST_IDLE;
                fsm_post_event(&(event_t){.id = EVT_ENTER_IDLE});
            }
            continue;
        }

        switch (state)
        {
        case ST_INIT:
            if (e.id == EVT_STARTUP)
            {
                servo_set_us(SERVO_CLOSE_US);
                state = ST_IDLE;
                fsm_post_event(&(event_t){.id = EVT_ENTER_IDLE});
            }
            break;

        case ST_IDLE:
            if (e.id == EVT_ENTER_IDLE)
            {
                memset(pin, 0, sizeof(pin));
                idx = 0;
                lcd_set_line1("PIN:");
                state = ST_ENTRADA_PIN;
            }
            break;

        case ST_ENTRADA_PIN:
            if (e.id == EVT_TECLA_DIGITO && idx < PIN_LENGTH)
            {
                pin[idx++] = e.tecla;
                char buf[17];
                snprintf(buf, 17, "PIN:%s", pin);
                lcd_set_line1(buf);
            }
            else if (e.id == EVT_TECLA_BORRAR && idx > 0)
            {
                idx--;
                pin[idx] = '\0';
                char buf[17];
                snprintf(buf, 17, "PIN:%s", pin);
                lcd_set_line1(buf);
            }
            else if (e.id == EVT_TECLA_CONFIRMAR)
            {
                lcd_set_line1("Validando...");
                if (!strcmp(pin, PIN_CORRECTO))
                {
                    lcd_set_line1("Abriendo...");
                    servo_set_us(SERVO_OPEN_US);
                    xTimerStart(t_servo, 0);
                    state = ST_ABIERTO;
                }
                else
                {
                    lcd_set_line1("ERROR");
                    xTimerStart(t_servo, 0);
                    state = ST_ERROR;
                }
            }
            break;

        case ST_ABIERTO:
        case ST_ERROR:
            if (e.id == EVT_TIMEOUT)
            {
                servo_set_us(SERVO_CLOSE_US);
                state = ST_IDLE;
                fsm_post_event(&(event_t){.id = EVT_ENTER_IDLE});
            }
            break;
        }
    }
}

/* ============================ TECLADO → FSM ============================ */

static void teclado_event_task(void *arg)
{
    char t;
    for (;;)
    {
        xQueueReceive(cola_teclas, &t, portMAX_DELAY);
        event_t e = {0};

        if (t >= '0' && t <= '9')      e.id = EVT_TECLA_DIGITO, e.tecla = t;
        else if (t == '#')             e.id = EVT_TECLA_CONFIRMAR;
        else if (t == '*')             e.id = EVT_TECLA_BORRAR;
        else continue;

        fsm_post_event(&e);
    }
}

/* ============================ LCD TASK ============================ */

static void lcd_task(void *arg)
{
    uint8_t h, m, s, d, mo;
    char l1[17], l2[21];

    xSemaphoreTake(mutex_i2c, portMAX_DELAY);
    lcd_init();
    xSemaphoreGive(mutex_i2c);

    for (;;)
    {
        ds3231_read(&h, &m, &s, &d, &mo);

        xSemaphoreTake(mutex_lcd, portMAX_DELAY);
        strcpy(l1, lcd_line1);
        xSemaphoreGive(mutex_lcd);

        snprintf(l2, 21, "%02d/%02d %02d:%02d:%02d ", d, mo, h, m, s);

        xSemaphoreTake(mutex_i2c, portMAX_DELAY);
        lcd_cmd(0x80);
        for (int i = 0; i < 16; i++) lcd_data(l1[i]);
        lcd_cmd(0xC0);
        for (int i = 0; i < 16; i++) lcd_data(l2[i]);
        xSemaphoreGive(mutex_i2c);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ============================ PANIC ============================ */

static void IRAM_ATTR panic_isr(void *arg)
{
    BaseType_t hpw = pdFALSE;
    xSemaphoreGiveFromISR(sem_panic, &hpw);
    if (hpw) portYIELD_FROM_ISR();
}

static void panic_task(void *arg)
{
    for (;;)
    {
        xSemaphoreTake(sem_panic, portMAX_DELAY);
        fsm_post_event(&(event_t){.id = EVT_PANIC});
    }
}

/* ============================ MAIN ============================ */

void app_main(void)
{
    q_events  = xQueueCreate(FSM_QUEUE_LEN, sizeof(event_t));
    sem_panic = xSemaphoreCreateBinary();
    mutex_i2c = xSemaphoreCreateMutex();
    mutex_lcd = xSemaphoreCreateMutex();

    teclado_init();
    i2c_init();
    servo_init();

    t_servo = xTimerCreate("servo", pdMS_TO_TICKS(SERVO_TIME_MS),
                           pdFALSE, NULL, servo_timeout_cb);

    gpio_install_isr_service(0);
    gpio_set_direction(PANIC_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PANIC_GPIO, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(PANIC_GPIO, GPIO_INTR_NEGEDGE);
    gpio_isr_handler_add(PANIC_GPIO, panic_isr, NULL);

    xTaskCreate(tarea_teclado, "kbd_scan", 2048, NULL, TECLADO_TASK_PRIO, NULL);
    xTaskCreate(teclado_event_task, "kbd_evt", 2048, NULL, TECLADO_EVENT_TASK_PRIO, NULL);
    xTaskCreate(fsm_task, "fsm", 4096, NULL, FSM_TASK_PRIO, NULL);
    xTaskCreate(panic_task, "panic", 2048, NULL, PANIC_TASK_PRIO, NULL);
    xTaskCreate(lcd_task, "lcd", 4096, NULL, LCD_TASK_PRIO, NULL);

    fsm_post_event(&(event_t){.id = EVT_STARTUP});
}
