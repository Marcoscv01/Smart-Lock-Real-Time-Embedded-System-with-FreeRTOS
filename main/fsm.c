#include "Teclado.h"
#include "lcd.h"
#include "servo.h"
#include "fsm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>
#include "esp_timer.h"

#define PIN_LENGTH       4
static const char PIN_CORRECTO[PIN_LENGTH+1] = "1234";

extern SemaphoreHandle_t sem_panic;
extern QueueHandle_t q_events;
extern QueueHandle_t cola_teclas;
extern TimerHandle_t t_servo;
extern fsm_state_t state;

//Envia a la cola el siguiente evento del FSM
static void fsm_post_event(event_t *e) {
    e->timestamp_us = esp_timer_get_time();
    xQueueSend(q_events, e, 0);
}

//Timeout que fuerza al siguiente evento
static void servo_timeout_cb(TimerHandle_t xTimer) {
    fsm_post_event(&(event_t){ .id = EVT_TIMEOUT });
}

//Incializa el FSM, creando el timeout del servo y comienza en evento STARTUP
void FSM_init(void){
    fsm_post_event(&(event_t){ .id = EVT_STARTUP });
    t_servo = xTimerCreate("servo", pdMS_TO_TICKS(SERVO_TIME_MS),
                           pdFALSE, NULL, servo_timeout_cb);
}

//Traduce cada tecla de la cola en el siguiente evento necesario
void Teclado2FSM_task(void *arg) {
    char t;
    for (;;) {
        xQueueReceive(cola_teclas, &t, portMAX_DELAY);
        event_t e = {0};

        if (t >= '0' && t <= '9') e.id = EVT_TECLA_DIGITO, e.tecla = t;
        else if (t == '#') e.id = EVT_TECLA_CONFIRMAR;
        else if (t == '*') e.id = EVT_TECLA_BORRAR;
        else continue;

        fsm_post_event(&e);
    }
}

//Evento para el PANIC
void panic_task(void *arg) {
    for (;;) {
        xSemaphoreTake(sem_panic, portMAX_DELAY);
        fsm_post_event(&(event_t){ .id = EVT_PANIC });
    }
}


/* ===== FSM ===== */
void fsm_task(void *arg) {
    char pin[PIN_LENGTH+1] = {0};
    uint8_t idx = 0;
    state = ST_INIT;

    for (;;) {
        event_t e;
        xQueueReceive(q_events, &e, portMAX_DELAY);

        if (e.id == EVT_PANIC) {
            if (state != ST_PANIC) {
                state = ST_PANIC;
                servo_set_us(SERVO_OPEN_US);
            } else {
                servo_set_us(SERVO_CLOSE_US);
                state = ST_IDLE;
                fsm_post_event(&(event_t){.id=EVT_ENTER_IDLE});
            }
            lcd_set_fsm_state(state, pin);
            continue;
        }

        switch (state) {
            case ST_INIT:
                if (e.id == EVT_STARTUP) {
                    servo_set_us(SERVO_CLOSE_US);
                    state = ST_IDLE;
                    fsm_post_event(&(event_t){.id=EVT_ENTER_IDLE});
                }
                break;

            case ST_IDLE:
                if (e.id == EVT_ENTER_IDLE) {
                    memset(pin, 0, sizeof(pin));
                    idx = 0;
                    state = ST_ENTRADA_PIN;
                }
                break;

            case ST_ENTRADA_PIN:
                if (e.id == EVT_TECLA_DIGITO && idx < PIN_LENGTH) {
                    pin[idx++] = e.tecla;
                    pin[idx] = '\0';
                } else if (e.id == EVT_TECLA_BORRAR && idx > 0) {
                    idx--;
                    pin[idx] = '\0';
                } else if (e.id == EVT_TECLA_CONFIRMAR) {
                    if (strcmp(pin, PIN_CORRECTO) == 0) {
                        state = ST_ABIERTO;
                        servo_set_us(SERVO_OPEN_US);
                        xTimerStart(t_servo, 0);
                    } else {
                        state = ST_ERROR;
                        xTimerStart(t_servo, 0);
                    }
                }
                break;

            case ST_ABIERTO:
            case ST_ERROR:
                if (e.id == EVT_TIMEOUT) {
                    servo_set_us(SERVO_CLOSE_US);
                    state = ST_IDLE;
                    fsm_post_event(&(event_t){.id=EVT_ENTER_IDLE});
                }
                break;

            case ST_PANIC:
                break;
        }

        lcd_set_fsm_state(state, pin);
    }
}