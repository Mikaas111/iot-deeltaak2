#include "speaker.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static void init_spiffs()
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE("MAIN", "SPIFFS init failed: %s", esp_err_to_name(err));
        return;
    }

    size_t total = 0;
    size_t used = 0;
    err = esp_spiffs_info(NULL, &total, &used);
    if (err == ESP_OK) {
        ESP_LOGI("MAIN", "SPIFFS mounted: total=%u used=%u", (unsigned)total, (unsigned)used);
    } else {
        ESP_LOGW("MAIN", "Could not read SPIFFS info: %s", esp_err_to_name(err));
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI("MAIN", "Starting app_main...");
    init_spiffs();
    ESP_LOGI("MAIN", "SPIFFS mounted, starting speaker");

    speaker speaker;
    esp_err_t err = speaker.init();
    if (err != ESP_OK) {
        ESP_LOGE("MAIN", "Speaker init failed: %s", esp_err_to_name(err));
        return;
    }

    while (true) {
        ESP_LOGI("MAIN", "Starting playback cycle...");
        err = speaker.play_wav_file("/spiffs/audio4.wav");
        if (err != ESP_OK) {
            ESP_LOGE("MAIN", "Playback failed: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(300));
    }
#include "board.h"
#include "mesh.h"
#include "ble_mesh_example_init.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"

static uint8_t dev_uuid[16] = {0xdd, 0xdd};
static mesh mesh_node(dev_uuid);

static void pin_monitor_task(void *pvParameters)
{
    bool last_state = board_is_input_high();

    for (;;) {
        bool curr = board_is_input_high();

        if (curr != last_state) {
            ESP_LOGI("MAIN", "GPIO14 changed: %d", curr);
            mesh_node.send_onoff(curr ? LED_ON : LED_OFF);
            last_state = curr;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

extern "C" void app_main()
{
    ESP_LOGI("MAIN", "Initializing...");

    board_init();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    err = bluetooth_init();
    if (err != ESP_OK) {
        ESP_LOGE("MAIN", "bluetooth_init failed");
        return;
    }

    ble_mesh_get_dev_uuid(dev_uuid);
    mesh_node.init();

    xTaskCreate(pin_monitor_task, "PinMon", 6144, NULL, 5, NULL);
}