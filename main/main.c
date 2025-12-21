#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "teclado.h"

QueueHandle_t cola_teclas;

void app_main(void)
{
    printf("\n--- PRUEBA TECLADO MATRICIAL ---\n");

    cola_teclas = xQueueCreate(10, sizeof(char));

    if (cola_teclas == NULL)
    {
        printf("Error creando la cola de teclas\n");
        return;
    }

    teclado_init();

    xTaskCreate(
        tarea_teclado,
        "tarea_teclado",
        2048,
        NULL,
        4,
        NULL);

    char tecla;

    while (1)
    {
        if (xQueueReceive(cola_teclas, &tecla, portMAX_DELAY))
        {
            printf("Tecla detectada: %c\n", tecla);
        }
    }
}
