#include "led.h"
#include "mesh.h"
#include "ble_mesh_example_init.h"
#include "speaker.h"
#include "ping.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "esp_timer.h"

#include <atomic>

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

    esp_vfs_spiffs_register(&conf);
}

static void aanOfUit(bool on)
{
    uint8_t value = on ? LED_ON : LED_OFF;
    setLed(value);
    mesh_node.send_onoff(value);
}

static void pingDetectieInterval(bool present)
{
    if (!present) return;

    g_last_detect_us.store(esp_timer_get_time());
    g_detection_active.store(true);

    aanOfUit(true);
}

static void pingTaak(void *pvParameters)
{
    (void)pvParameters;

    static constexpr int64_t HOLD_US = 5000000;

    for (;;) {
        ping_sensor.update();

        if (g_detection_active.load()) {
            int64_t last = g_last_detect_us.load();
            if (last > 0) {
                int64_t now = esp_timer_get_time();
                if ((now - last) >= HOLD_US) {
                    g_detection_active.store(false);
                    g_last_detect_us.store(-1);
                    aanOfUit(false);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void knopTaak(void *pvParameters)
{
    (void)pvParameters;

    bool last_state = board_is_input_high();

    for (;;) {
        bool curr = board_is_input_high();

        if (curr != last_state) {
            uint8_t value = curr ? LED_ON : LED_OFF;
            setLed(value);
            mesh_node.send_onoff(value);
            last_state = curr;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void audioTaak(void *pvParameters)
{
    (void)pvParameters;

    for (;;) {
        if (!isLedAan()) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        audio_speaker.playWavFile("/spiffs/audio4.wav");

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

extern "C" void app_main(void)
{
    board_init();

    nvs_flash_init();
    init_spiffs();

    audio_speaker.init();
    bluetooth_init();

    ble_mesh_get_dev_uuid(dev_uuid);
    mesh_node.init();

    ping_sensor.init();
    ping_sensor.set_presence_callback(pingDetectieInterval);

    xTaskCreate(knopTaak, "knopTaak", 4096, nullptr, 5, nullptr);
    xTaskCreate(pingTaak, "pingTaak", 4096, nullptr, 5, nullptr);
    xTaskCreate(audioTaak, "audioTaak", 6144, nullptr, 5, nullptr);
}