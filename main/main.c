#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "teclado.h"

//Declaracion de contraseña
#define PIN_LENGTH 4
static const char PIN_CORRECTO[PIN_LENGTH + 1] = "1234";

QueueHandle_t cola_teclas;

void app_main(void)
{
    printf("\n--- Logica Teclado ---\n");

    cola_teclas = xQueueCreate(10, sizeof(char));

    if (cola_teclas == NULL)
    {
        printf("Error creando la cola de teclas\n");
        return;
    }

    teclado_init();
    xTaskCreate(tarea_teclado,"tarea_teclado",2048,NULL,4,NULL);

    char pin_buffer[PIN_LENGTH + 1] = {0};
    uint8_t pin_index = 0;

    char tecla;

    while (1)
    {
        if (xQueueReceive(cola_teclas, &tecla, portMAX_DELAY))
        {
            //Borrar digito
            if (tecla == '*')
            {
                if (pin_index > 0)
                {
                    pin_index--;
                    pin_buffer[pin_index] = '\0';
                }
                printf("\nPIN actual: %s\n", pin_buffer);
            }
            // Confirmar PIN
            else if (tecla == '#')
            {
                printf("\nPIN introducido: %s\n", pin_buffer);
                if (strcmp(pin_buffer, PIN_CORRECTO) == 0)
                {
                    printf("\nACCESO CONCEDIDO\n");
                }
                else
                {
                    printf("\nACCESO DENEGADO\n");
                }
                // Reset PIN
                memset(pin_buffer, 0, sizeof(pin_buffer));
                pin_index = 0;
            }
            // Digitos normales
            else
            {
                if (pin_index < PIN_LENGTH)
                {
                    pin_buffer[pin_index++] = tecla;
                    pin_buffer[pin_index] = '\0';
                    printf("\nPIN actual: %s\n", pin_buffer);
                }
            }
        }
    }
}
