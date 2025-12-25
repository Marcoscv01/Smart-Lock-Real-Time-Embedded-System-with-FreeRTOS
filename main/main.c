#include "Teclado.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "esp_log.h"

#define PIN_LENGTH       4
static const char *TAG = "fsm_acceso";
static const char PIN_CORRECTO[PIN_LENGTH + 1] = "1234";

#define SERVO_GPIO       15
#define SERVO_OPEN_US    2000
#define SERVO_CLOSE_US   1000

#define FSM_QUEUE_LEN    10
#define FSM_TASK_STACK   4096
#define FSM_TASK_PRIO    5

//Estados del FSM
typedef enum {
    ST_INIT = 0, 
    ST_IDLE,
    ST_ENTRADA_PIN,
    ST_VALIDANDO,
    ST_ABIERTO,
    ST_ERROR
} fsm_state_t;

//Eventos del FSM
typedef enum {
    EVT_NONE = 0,
    EVT_STARTUP,
    EVT_ENTER_IDLE,
    EVT_TECLA_DIGITO,
    EVT_TECLA_BORRAR,
    EVT_TECLA_CONFIRMAR
} event_id_t;

/* ============================ Evento FSM ============================ */
typedef struct {
    event_id_t id;
    char tecla;
    int64_t timestamp_us;
} event_t;

/* ============================ Recursos globales ============================ */
QueueHandle_t cola_teclas;        
static QueueHandle_t q_events;
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

/* ============================ Envío de eventos a FSM ============================ */
static inline void fsm_post_event(event_t *e)
{
    e->timestamp_us = esp_timer_get_time();
    xQueueSend(q_events, e, 0);
}


/* ============================ Tarea FSM ============================ */
static void fsm_task(void *arg)
{
    char pin_buffer[PIN_LENGTH + 1] = {0};
    uint8_t pin_index = 0;

    state = ST_INIT;
    ESP_LOGI(TAG, "FSM en ST_INIT");

    TickType_t lastWakeTime = xTaskGetTickCount();  

    for (;;)
    {
        event_t e;
        if (xQueueReceive(q_events, &e, portMAX_DELAY) != pdTRUE)
            continue;

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
            if(e.id == EVT_TECLA_DIGITO && pin_index < PIN_LENGTH)
            {
                pin_buffer[pin_index++] = e.tecla;
                pin_buffer[pin_index] = '\0';
                ESP_LOGI(TAG, "[PIN] %s", pin_buffer);
            }
            else if(e.id == EVT_TECLA_BORRAR)
            {
                if(pin_index > 0)
                {
                    pin_index--;
                    pin_buffer[pin_index] = '\0';
                }
                ESP_LOGI(TAG, "[PIN] %s", pin_buffer);  
            }
            else if(e.id == EVT_TECLA_CONFIRMAR)
            {
                if(pin_index > 0)
                {
                    ESP_LOGI(TAG, "[VALIDANDO] %s", pin_buffer);
                    if(strcmp(pin_buffer, PIN_CORRECTO) == 0)
                    {
                        state = ST_ABIERTO;
                        ESP_LOGI(TAG, "[ABIERTO] Acceso concedido");
                        servo_set_us(SERVO_OPEN_US);
                        vTaskDelay(pdMS_TO_TICKS(3000));
                        servo_set_us(SERVO_CLOSE_US);
                        state = ST_IDLE;

                        event_t back = {.id = EVT_ENTER_IDLE};
                        fsm_post_event(&back);
                    }
                    else
                    {
                        state = ST_ERROR;
                        ESP_LOGW(TAG, "[ERROR] Acceso denegado");
                        vTaskDelay(pdMS_TO_TICKS(2000));
                        state = ST_IDLE;

                        event_t retry = {.id = EVT_ENTER_IDLE};
                        fsm_post_event(&retry);
                    }

                    pin_index = 0;
                    memset(pin_buffer, 0, sizeof(pin_buffer));
                }
            }
            break;

        case ST_ABIERTO:
        case ST_ERROR:
        case ST_VALIDANDO:
            break;
        }

        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(10)); 
    }
}

/* ============================ Adaptador teclado → FSM ============================ */
static void teclado_event_task(void *arg)
{
    char tecla;
    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        if (xQueueReceive(cola_teclas, &tecla, portMAX_DELAY))
        {
            event_t e = {0};

            if(tecla >= '0' && tecla <= '9')      { e.id = EVT_TECLA_DIGITO; e.tecla = tecla; }
            else if(tecla == '*')                 { e.id = EVT_TECLA_BORRAR; }
            else if(tecla == '#')                 { e.id = EVT_TECLA_CONFIRMAR; }
            else                                  { continue; }

            fsm_post_event(&e);
        
        }

        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(50));
    }
}


/* ============================ app_main ============================ */
void app_main(void)
{
    ESP_LOGI(TAG, "Control de acceso FSM (modelo por eventos)");

    // Inicializar teclado (configura pines y crea cola)
    teclado_init();

    // Crear tarea de escaneo de teclado
    xTaskCreate(tarea_teclado, "kbd_scan", 2048, NULL, 6, NULL);

    // Crear cola de eventos FSM
    q_events = xQueueCreate(FSM_QUEUE_LEN, sizeof(event_t));
    configASSERT(q_events);

    // Inicializar servo
    servo_init();
    servo_set_us(SERVO_CLOSE_US);

    // Crear tareas FSM y adaptador teclado
    xTaskCreate(fsm_task, "fsm", FSM_TASK_STACK, NULL, 4, NULL);
    xTaskCreate(teclado_event_task, "kbd_evt", 2048, NULL, 5, NULL);

    // Evento de inicio
    event_t start = {.id = EVT_STARTUP};
    fsm_post_event(&start);
}
