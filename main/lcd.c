#include "lcd.h"
#include "rtc.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "fsm.h"

#define I2C_PORT     I2C_NUM_0
#define PCF8574_ADDR 0x27

static SemaphoreHandle_t mutex_lcd;
static char lcd_line1[17] = "Inicializando";
static char lcd_line2[17] = "";

extern QueueHandle_t q_fsm_lcd;

/* ===== HW LCD ===== */
static void lcd_delay(void){ vTaskDelay(pdMS_TO_TICKS(5)); }

static void pcf_write(uint8_t d){
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, PCF8574_ADDR<<1, true);
    i2c_master_write_byte(cmd, d, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
}

static void lcd_nibble(uint8_t n, bool rs){
    uint8_t d = (n<<4) | 0x08 | (rs?1:0);
    pcf_write(d | 0x04); lcd_delay();
    pcf_write(d & ~0x04); lcd_delay();
}

static void lcd_cmd(uint8_t c){
    lcd_nibble(c>>4,false);
    lcd_nibble(c&0x0F,false);
}

static void lcd_data(uint8_t d){
    lcd_nibble(d>>4,true);
    lcd_nibble(d&0x0F,true);
}

static void lcd_hw_init(void){
    lcd_delay();
    lcd_nibble(0x03,false);
    lcd_nibble(0x03,false);
    lcd_nibble(0x03,false);
    lcd_nibble(0x02,false);
    lcd_cmd(0x28);
    lcd_cmd(0x0C);
    lcd_cmd(0x01);
    lcd_cmd(0x06);
}

//inicializa el LCD, creando mutex interno
void LCD_init(void){
    mutex_lcd = xSemaphoreCreateMutex();
    lcd_hw_init();

}

//Coge TIEMPO y por I2C lo imprime por el LCD
void lcd_task(void *arg)
{
    

    for (;;)
    {
        char l2[17];
        xSemaphoreTake(mutex_time, portMAX_DELAY);

        if (rtc_time.valid) {
            snprintf(l2, 21,
                     "%02d/%02d %02d:%02d:%02d",
                     rtc_time.day,
                     rtc_time.month,
                     rtc_time.hour,
                     rtc_time.min,
                     rtc_time.sec);
        } else {
            snprintf(l2, sizeof(l2), "RTC NO VALIDO ");
        }

        xSemaphoreGive(mutex_time);

        fsm_display_t msg;
        if (xQueueReceive(q_fsm_lcd, &msg, 0) == pdTRUE) {
            xSemaphoreTake(mutex_lcd, portMAX_DELAY);
            switch (msg.state) {
                case ST_INIT:        snprintf(lcd_line1,17,"Inicializando"); break;
                case ST_IDLE:        snprintf(lcd_line1,17,"Esperando PIN"); break;
                case ST_ENTRADA_PIN: snprintf(lcd_line1,17,"PIN: %-11s", msg.pin); break;
                case ST_ABIERTO:     snprintf(lcd_line1,17,"Abriendo..."); break;
                case ST_PANIC:       snprintf(lcd_line1,17,"!!! PANIC !!!"); break;
                case ST_ERROR:       snprintf(lcd_line1,17,"ERROR"); break;
            }
            xSemaphoreGive(mutex_lcd);
        }


        xSemaphoreTake(mutex_lcd, portMAX_DELAY);
        lcd_cmd(0x80);
        for (int i = 0; i < 16; i++) lcd_data(lcd_line1[i]);
        lcd_cmd(0xC0);
        for (int i = 0; i < 16; i++) lcd_data(l2[i]);
        xSemaphoreGive(mutex_lcd);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
