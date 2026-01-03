#define TECLADO_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include <string.h>

#define NUM_FILAS 4
#define NUM_COLUMNAS 4

// Pines definidos por el usuario
static const gpio_num_t filas[NUM_FILAS] = {13, 12, 14, 27};
static const gpio_num_t columnas[NUM_COLUMNAS] = {26, 25, 33, 32};

// Mapa del teclado 4x4
static const char mapa_teclas[NUM_FILAS][NUM_COLUMNAS] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};

// Cola donde se envían las teclas detectadas
extern QueueHandle_t cola_teclas;

// Inicialización del teclado
static inline void teclado_init(void)
{
    cola_teclas = xQueueCreate(10, sizeof(char));
    configASSERT(cola_teclas);


    gpio_config_t io_conf = {0};

    // Configurar filas como salida
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 0;
    for (int i = 0; i < NUM_FILAS; i++)
        io_conf.pin_bit_mask |= (1ULL << filas[i]);
    gpio_config(&io_conf);

    // Configurar columnas como entrada con pull-up
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pin_bit_mask = 0;
    for (int i = 0; i < NUM_COLUMNAS; i++)
        io_conf.pin_bit_mask |= (1ULL << columnas[i]);
    gpio_config(&io_conf);

    // Poner todas las filas en alto
    for (int i = 0; i < NUM_FILAS; i++)
        gpio_set_level(filas[i], 1);
}

// Tarea de escaneo del teclado
static inline void tarea_teclado(void *pvParameters)
{
    char tecla;

    while (1)
    {
        for (int f = 0; f < NUM_FILAS; f++)
        {
            // Activar fila
            gpio_set_level(filas[f], 0);

            for (int c = 0; c < NUM_COLUMNAS; c++)
            {
                if (gpio_get_level(columnas[c]) == 0)
                {
                    tecla = mapa_teclas[f][c];
                    xQueueSend(cola_teclas, &tecla, portMAX_DELAY);

                    // Debounce simple por delay
                    vTaskDelay(pdMS_TO_TICKS(200));

                    // Esperar a que se suelte la tecla
                    while (gpio_get_level(columnas[c]) == 0)
                        vTaskDelay(pdMS_TO_TICKS(10));
                }
            }

            // Desactivar fila
            gpio_set_level(filas[f], 1);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

