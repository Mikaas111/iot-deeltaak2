#include "ping.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define TRIG_PIN GPIO_NUM_16
#define ECHO_PIN GPIO_NUM_15
#define LED_PIN  GPIO_NUM_2

static ping sensor(TRIG_PIN, ECHO_PIN, LED_PIN, 20.0f);

extern "C" void app_main(void)
{
    ESP_LOGI("MAIN", "Starting ping sensor app...");

    if (sensor.init() != ESP_OK) {
        ESP_LOGE("MAIN", "Failed to init sensor");
        return;
    }

    while (true) {
        sensor.update();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}