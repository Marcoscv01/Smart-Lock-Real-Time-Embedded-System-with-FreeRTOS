#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef enum {
    ST_INIT = 0,
    ST_IDLE,
    ST_ENTRADA_PIN,
    ST_ABIERTO,
    ST_PANIC,
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

void FSM_init(void);
void Teclado2FSM_task(void *arg);
void fsm_task(void *arg);
void panic_task(void *arg);