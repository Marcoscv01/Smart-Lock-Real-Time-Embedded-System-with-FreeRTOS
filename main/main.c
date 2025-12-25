#include "Teclado.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>

/* ============================ Configuración ============================ */

#define PIN_LENGTH       4
static const char *TAG = "fsm_acceso";
static const char PIN_CORRECTO[PIN_LENGTH + 1] = "1234";

#define SERVO_GPIO GPIO_NUM_15
#define SERVO_OPEN_US    2000
#define SERVO_CLOSE_US   1000
#define SERVO_TIME_MS    3000

#define FSM_QUEUE_LEN    10
#define FSM_TASK_STACK   4096

#define FSM_TASK_PRIO    5
#define Teclado_Event_TASK_PRIO    4
#define Teclado_TASK_PRIO    6

#define PANIC_GPIO GPIO_NUM_0

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
    EVT_PANIC_ISR

} event_id_t;

typedef struct {
    event_id_t id;
    char tecla;
    int64_t timestamp_us;
} event_t;

/* ============================ Recursos globales ============================ */

QueueHandle_t cola_teclas;
static QueueHandle_t q_events;
static TimerHandle_t t_servo;
static fsm_state_t state;

/* ============================ Servo ============================ */

static void servo_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_16_BIT,
        .freq_hz = 50
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t channel = {
        .gpio_num = SERVO_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0
    };
    ledc_channel_config(&channel);
}

static void servo_set_us(uint32_t us)
{
    uint32_t duty = (us * 65535) / 20000;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

/* ============================ Eventos FSM ============================ */

static inline void fsm_post_event(event_t *e)
{
    e->timestamp_us = esp_timer_get_time();
    xQueueSend(q_events, e, 0);
}

static inline void fsm_post_event_isr(event_t *e)
{
    BaseType_t hpw = pdFALSE;
    e->timestamp_us = esp_timer_get_time();
    xQueueSendFromISR(q_events, e, &hpw);
    if (hpw == pdTRUE)
        portYIELD_FROM_ISR();
}

/* ============================ Timer Servo ============================ */

static void servo_timeout_cb(TimerHandle_t xTimer)
{
    event_t e = {.id = EVT_TIMEOUT};
    fsm_post_event(&e);
}

/* ============================ FSM Task ============================ */

static void fsm_task(void *arg)
{
    char pin_buffer[PIN_LENGTH + 1] = {0};
    uint8_t pin_index = 0;

    state = ST_INIT;
    ESP_LOGI(TAG, "FSM en ST_INIT");

    for (;;)
    {
        event_t e;
        if (xQueueReceive(q_events, &e, portMAX_DELAY) != pdTRUE)
            continue;

        // ==== Manejo de pánico toggle ====
        if (e.id == EVT_PANIC_ISR)
        {
            if (state != ST_ERROR)
            {
                ESP_LOGE(TAG, "[PANIC] Activado");
                servo_set_us(SERVO_OPEN_US);
                state = ST_ERROR;
            }
            else
            {
                ESP_LOGI(TAG, "[PANIC] Desactivado");
                servo_set_us(SERVO_CLOSE_US);
                state = ST_IDLE;
                event_t back = {.id = EVT_ENTER_IDLE};
                fsm_post_event(&back);
            }
            continue;
        }

        // ==== FSM según estado ====
        switch (state)
        {
            case ST_INIT:
                if (e.id == EVT_STARTUP)
                {
                    servo_set_us(SERVO_CLOSE_US);
                    ESP_LOGI(TAG, "[INIT] -> IDLE");
                    state = ST_IDLE;

                    event_t next = {.id = EVT_ENTER_IDLE};
                    fsm_post_event(&next);
                }
                break;

            case ST_IDLE:
                if (e.id == EVT_ENTER_IDLE)
                {
                    memset(pin_buffer, 0, sizeof(pin_buffer));
                    pin_index = 0;
                    ESP_LOGI(TAG, "[IDLE] Introducir PIN");
                    state = ST_ENTRADA_PIN;
                }
                break;

            case ST_ENTRADA_PIN:
                if (e.id == EVT_TECLA_DIGITO && pin_index < PIN_LENGTH)
                {
                    pin_buffer[pin_index++] = e.tecla;
                    pin_buffer[pin_index] = '\0';
                    ESP_LOGI(TAG, "[PIN] %s", pin_buffer);
                }
                else if (e.id == EVT_TECLA_BORRAR && pin_index > 0)
                {
                    pin_index--;
                    pin_buffer[pin_index] = '\0';
                    ESP_LOGI(TAG, "[PIN] %s", pin_buffer);
                }
                else if (e.id == EVT_TECLA_CONFIRMAR)
                {
                    ESP_LOGI(TAG, "[VALIDANDO] %s", pin_buffer);

                    if (strcmp(pin_buffer, PIN_CORRECTO) == 0)
                    {
                        ESP_LOGI(TAG, "[ABIERTO] Acceso concedido");
                        servo_set_us(SERVO_OPEN_US);
                        xTimerStart(t_servo, 0);
                        state = ST_ABIERTO;
                    }
                    else
                    {
                        ESP_LOGW(TAG, "[ERROR] Acceso denegado");
                        xTimerStart(t_servo, 0);
                        state = ST_ERROR;
                    }

                    memset(pin_buffer, 0, sizeof(pin_buffer));
                    pin_index = 0;
                }
                break;

            case ST_ABIERTO:
                if (e.id == EVT_TIMEOUT)
                {
                    ESP_LOGI(TAG, "[ABIERTO] Cerrando servo");
                    servo_set_us(SERVO_CLOSE_US);
                    state = ST_IDLE;

                    event_t back = {.id = EVT_ENTER_IDLE};
                    fsm_post_event(&back);
                }
                break;

            case ST_ERROR:
                // Ahora no hacemos nada con timeout; solo sale cuando se pulsa el botón
                break;

            default:
                ESP_LOGW(TAG, "Estado desconocido: %d", state);
                state = ST_IDLE;
                break;
        }
    }
}



/* ============================ Adaptador Teclado → FSM ============================ */

static void teclado_event_task(void *arg)
{
    char tecla;

    for (;;)
    {
        if (xQueueReceive(cola_teclas, &tecla, portMAX_DELAY))
        {
            event_t e = {0};

            if (tecla >= '0' && tecla <= '9')
                e.id = EVT_TECLA_DIGITO, e.tecla = tecla;
            else if (tecla == '*')
                e.id = EVT_TECLA_BORRAR;
            else if (tecla == '#')
                e.id = EVT_TECLA_CONFIRMAR;
            else
                continue;

            fsm_post_event(&e);
        }
    }
}

/* ============================ ISR Panico ============================ */
static void IRAM_ATTR panic_isr(void *arg)
{
    event_t e = {.id = EVT_PANIC_ISR};
    fsm_post_event_isr(&e);
}

static void panic_button_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PANIC_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE 
    };
    gpio_config(&io);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PANIC_GPIO, panic_isr, NULL);

    ESP_LOGI(TAG, "Botón de pánico en GPIO %d", PANIC_GPIO);
}

/* ============================ app_main ============================ */

void app_main(void)
{
    ESP_LOGI(TAG, "Control de acceso FSM (no bloqueante)");
    panic_button_init();

    teclado_init();
    xTaskCreate(tarea_teclado, "kbd_scan", 2048, NULL, Teclado_TASK_PRIO, NULL);

    q_events = xQueueCreate(FSM_QUEUE_LEN, sizeof(event_t));
    configASSERT(q_events);

    servo_init();
    servo_set_us(SERVO_CLOSE_US);

    t_servo = xTimerCreate(
        "servo_timer",
        pdMS_TO_TICKS(SERVO_TIME_MS),
        pdFALSE,
        NULL,
        servo_timeout_cb
    );
    configASSERT(t_servo);

    xTaskCreate(fsm_task, "fsm", FSM_TASK_STACK, NULL, FSM_TASK_PRIO, NULL);
    xTaskCreate(teclado_event_task, "kbd_evt", 2048, NULL, Teclado_Event_TASK_PRIO, NULL);

    event_t start = {.id = EVT_STARTUP};
    fsm_post_event(&start);
}
