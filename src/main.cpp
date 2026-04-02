#include "board.h"
#include "mesh.h"
#include "ble_mesh_example_init.h"
#include "speaker.h"
#include "ping.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_err.h"
#include "esp_timer.h"

#include <atomic>

static const char *TAG = "MAIN";

static uint8_t dev_uuid[16] = {0xdd, 0xdd};
static mesh mesh_node(dev_uuid);
static speaker audio_speaker;

// Pas deze GPIO’s aan naar jouw bekabeling
static ping ping_sensor(GPIO_NUM_16, GPIO_NUM_15, 20.0f, 5000);

static std::atomic<bool> g_detection_active{false};
static std::atomic<int64_t> g_last_detect_us{-1};

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

static void set_output_state(bool on)
{
    uint8_t value = on ? LED_ON : LED_OFF;
    board_set_led(value);
    mesh_node.send_onoff(value);
}

static void ping_presence_callback(bool present)
{
    if (!present) {
        return;
    }

    g_last_detect_us.store(esp_timer_get_time());
    g_detection_active.store(true);

    set_output_state(true);
}

static void ping_task(void *pvParameters)
{
    (void)pvParameters;

    static constexpr int64_t HOLD_US = 5000000; // 5 sec

    for (;;) {
        ping_sensor.update();

        if (g_detection_active.load()) {
            int64_t last = g_last_detect_us.load();
            if (last > 0) {
                int64_t now = esp_timer_get_time();
                if ((now - last) >= HOLD_US) {
                    // timeout voorbij: alles uit
                    g_detection_active.store(false);
                    g_last_detect_us.store(-1);

                    set_output_state(false);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void input_task(void *pvParameters)
{
    (void)pvParameters;

    bool last_state = board_is_input_high();

    for (;;) {
        bool curr = board_is_input_high();

        if (curr != last_state) {
            ESP_LOGI(TAG, "INPUT_PIN changed: %d", curr);

            uint8_t value = curr ? LED_ON : LED_OFF;
            board_set_led(value);
            mesh_node.send_onoff(value);

            last_state = curr;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
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

        ESP_LOGI(TAG, "Starting playback...");
        esp_err_t err = audio_speaker.play_wav_file("/spiffs/audio4.wav");
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Playback stopped/failed: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(100));
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

    err = ping_sensor.init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Ping sensor init failed: %s", esp_err_to_name(err));
        return;
    }

    ping_sensor.set_presence_callback(ping_presence_callback);

    xTaskCreate(input_task, "InputTask", 4096, nullptr, 5, nullptr);
    xTaskCreate(ping_task, "PingTask", 4096, nullptr, 5, nullptr);
    xTaskCreate(audio_task, "AudioTask", 6144, nullptr, 5, nullptr);
}