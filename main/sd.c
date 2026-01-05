#include "sd.h"
#include "fsm.h"
#include "rtc.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

#include <stdio.h>
#include <string.h>

/* ===== CONFIG ===== */
#define PIN_MOSI 22
#define PIN_MISO 23
#define PIN_SCK  4
#define PIN_CS   2

#define MOUNT_POINT "/sdcard"
#define LOG_FILE    "/sdcard/log.txt"

#define TAG "SD"

/* ===== EXTERN ===== */
extern QueueHandle_t q_fsm_sd;
extern SemaphoreHandle_t mutex_time;
extern rtc_time_t rtc_time;

/* ===== INIT SD (TU CÓDIGO, intacto) ===== */
static void send_dummy_clocks(void)
{
    gpio_set_level(PIN_CS, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    for (int i = 0; i < 10; i++) {
    gpio_set_level(PIN_SCK, 0);
    esp_rom_delay_us(10);
    gpio_set_level(PIN_SCK, 1);
    esp_rom_delay_us(10);
}

}

void SPI_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Inicializando pines SPI...");

    // Configurar pines
    gpio_reset_pin(PIN_CS);
    gpio_set_direction(PIN_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_CS, 1);

    gpio_pullup_en(PIN_MISO);
    gpio_pullup_en(PIN_MOSI);
    gpio_pullup_en(PIN_SCK);
    gpio_pullup_en(PIN_CS);

    // Enviar clocks dummy
    send_dummy_clocks();

    // Configurar bus SPI
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error inicializando SPI: %s", esp_err_to_name(ret));
        return;
    }

    // Configuración host SD
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    host.max_freq_khz = 10; // inicio lento, luego subir

    // Configuración dispositivo SPI
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_CS;
    slot_config.host_id = SPI2_HOST;

    // Configuración FATFS
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    }; 

    sdmmc_card_t *card;

    ESP_LOGI(TAG, "Montando SD por SPI...");
    ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error montando SD: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "SD montada correctamente!");
    sdmmc_card_print_info(stdout, card);

    // Subir frecuencia a 4 MHz después de montar
    host.max_freq_khz = 4000;
}

/* ===== LOG ===== */
static void sd_log_event(const char *event)
{
    if (!rtc_time.valid) return;

    xSemaphoreTake(mutex_time, portMAX_DELAY);

    char line[128];
    snprintf(line, sizeof(line),
             "%04d-%02d-%02d %02d:%02d:%02d - %s\n",
             rtc_time.year, rtc_time.month, rtc_time.day,
             rtc_time.hour, rtc_time.min, rtc_time.sec,
             event);

    xSemaphoreGive(mutex_time);

    FILE *f = fopen(LOG_FILE, "a");
    if (f) {
        fputs(line, f);
        fclose(f);
    }
}

/* ===== SD TASK ===== */
void sd_task(void *arg)
{
    fsm_display_t msg;
    fsm_state_t last_state = ST_INIT;

    for (;;) {
        xQueueReceive(q_fsm_sd, &msg, portMAX_DELAY);

        if (msg.state != last_state) {
            switch (msg.state) {
                case ST_ABIERTO:
                    sd_log_event("Puerta abierta");
                    break;

                case ST_ERROR:
                    sd_log_event("Acceso fallido");
                    break;

                case ST_PANIC:
                    sd_log_event("PANICO ON");
                    break;

                default:
                    break;
            }

            if (last_state == ST_PANIC && msg.state != ST_PANIC) {
                sd_log_event("PANICO OFF");
            }

            last_state = msg.state;
        }
    }
}
