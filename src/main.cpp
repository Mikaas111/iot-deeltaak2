#include "board.h"
#include "mesh.h"
#include "ble_mesh_example_init.h"
#include "speaker.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_err.h"

static const char *TAG = "MAIN";

static uint8_t dev_uuid[16] = {0xdd, 0xdd};
static mesh mesh_node(dev_uuid);
static speaker audio_speaker;

static void init_spiffs()
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = nullptr,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS init failed: %s", esp_err_to_name(err));
        return;
    }

    size_t total = 0;
    size_t used = 0;
    err = esp_spiffs_info(nullptr, &total, &used);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS mounted: total=%u used=%u",
                 static_cast<unsigned>(total),
                 static_cast<unsigned>(used));
    } else {
        ESP_LOGW(TAG, "Could not read SPIFFS info: %s", esp_err_to_name(err));
    }
}

static void audio_task(void *pvParameters)
{
    (void)pvParameters;

    for (;;) {
        if (!board_is_led_on()) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        ESP_LOGI(TAG, "Starting playback cycle...");
        esp_err_t err = audio_speaker.play_wav_file("/spiffs/audio4.wav");
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Playback stopped/failed: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void pin_monitor_task(void *pvParameters)
{
    (void)pvParameters;

    bool last_state = board_is_input_high();

    for (;;) {
        bool curr = board_is_input_high();

        if (curr != last_state) {
            ESP_LOGI(TAG, "GPIO14 changed: %d", curr);

            uint8_t led_value = curr ? LED_ON : LED_OFF;
            board_set_led(led_value);
            mesh_node.send_onoff(led_value);

            last_state = curr;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting app_main...");

    board_init();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    init_spiffs();

    err = audio_speaker.init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Speaker init failed: %s", esp_err_to_name(err));
        return;
    }

    err = bluetooth_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bluetooth_init failed: %s", esp_err_to_name(err));
        return;
    }

    ble_mesh_get_dev_uuid(dev_uuid);
    mesh_node.init();

    xTaskCreate(audio_task, "AudioTask", 6144, nullptr, 5, nullptr);
    xTaskCreate(pin_monitor_task, "PinMon", 4096, nullptr, 5, nullptr);
}