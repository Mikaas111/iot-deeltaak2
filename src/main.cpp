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