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
}