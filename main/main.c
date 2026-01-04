#include "Teclado.h"
#include "lcd.h"
#include "rtc.h"
#include "servo.h"
#include "fsm.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"
#include "driver/i2c.h"

/* CONFIG */
#define PANIC_GPIO       GPIO_NUM_0

#define FSM_QUEUE_LEN           10
//Prioridades tareas
#define FSM_TASK_PRIO           5
#define TECLADO_EVENT_TASK_PRIO 4
#define TECLADO_TASK_PRIO       6
#define PANIC_TASK_PRIO         7
#define LCD_TASK_PRIO           3
#define RTC_TASK_PRIO           4

/* ===== RECURSOS ===== */
QueueHandle_t cola_teclas;
QueueHandle_t q_events;
TimerHandle_t t_servo;
SemaphoreHandle_t sem_panic;

SemaphoreHandle_t mutex_i2c;
SemaphoreHandle_t mutex_time;
rtc_time_t rtc_time;

fsm_state_t state;

/* ===== ISR PANIC ===== */
static void IRAM_ATTR panic_isr(void *arg) {
    BaseType_t hpw = pdFALSE;
    xSemaphoreGiveFromISR(sem_panic, &hpw);
    if (hpw) portYIELD_FROM_ISR();
}
 
/* ===== MAIN ===== */
void app_main(void) {
    //Creacion recursos
    q_events  = xQueueCreate(FSM_QUEUE_LEN, sizeof(event_t));
    sem_panic = xSemaphoreCreateBinary();
    mutex_i2c = xSemaphoreCreateMutex();
    mutex_time = xSemaphoreCreateMutex();
    cola_teclas = xQueueCreate(10, sizeof(char));

    rtc_time.valid = false;

    //Bus i2c
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 19,
        .scl_io_num = 18,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000
    };
    i2c_param_config(I2C_NUM_0, &i2c_conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

// Boton ISR
    gpio_install_isr_service(0);
    gpio_set_direction(PANIC_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PANIC_GPIO, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(PANIC_GPIO, GPIO_INTR_NEGEDGE);
    gpio_isr_handler_add(PANIC_GPIO, panic_isr, NULL);

    //Inicializacion de encapsulados
    teclado_init();
    servo_init();
    LCD_init();
    FSM_init();

    //Tareas
    xTaskCreate(Teclado_Scan_task, "kbd_scan", 2048, NULL, TECLADO_TASK_PRIO, NULL);
    xTaskCreate(Teclado2FSM_task, "kbd_evt", 2048, NULL, TECLADO_EVENT_TASK_PRIO, NULL);
    xTaskCreate(fsm_task, "fsm", 4096, NULL, FSM_TASK_PRIO, NULL);
    xTaskCreate(panic_task, "panic", 2048, NULL, PANIC_TASK_PRIO, NULL);
    xTaskCreate(lcd_task, "lcd", 4096, NULL, LCD_TASK_PRIO, NULL);
    xTaskCreate(rtc_task, "rtc", 2048, NULL, RTC_TASK_PRIO, NULL);

    
}
