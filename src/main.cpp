#include "board.h"
#include "mesh.h"
#include "button.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"

#define BUTTON_PIN GPIO_NUM_14

static uint8_t dev_uuid[16] = {0xdd, 0xdd};
static mesh mesh(dev_uuid);

void button_pressed() {
    ESP_LOGI("MAIN", "Button pressed, sending broadcast");
    mesh.send_onoff(LED_ON);
}

extern "C" void app_main() {
    ESP_LOGI("MAIN", "Initializing...");

    board_init();
    nvs_flash_init();

    mesh.init();

    button btn(BUTTON_PIN, button_pressed);

    while (true) {
        btn.poll();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}