/* board.c - Board-specific hooks */

#include <stdio.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "board.h"

#define TAG "BOARD"

struct _led_state led_state[1] = {
    { LED_OFF, LED_OFF, LED_PIN, "led" },
};

void board_led_operation(uint8_t pin, uint8_t onoff)
{
    for (int i = 0; i < 1; i++) {
        if (led_state[i].pin != pin) {
            continue;
        }

        if (onoff == led_state[i].previous) {
            return;
        }

        gpio_set_level(pin, onoff);
        led_state[i].previous = onoff;
        led_state[i].current = onoff;
        return;
    }

    ESP_LOGE(TAG, "LED is not found!");
}

void board_set_led(uint8_t onoff)
{
    board_led_operation(LED_PIN, onoff);
}

static void board_led_init(void)
{
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, LED_OFF);
    led_state[0].previous = LED_OFF;
    led_state[0].current = LED_OFF;
}

void board_init(void)
{
    board_led_init();

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << INPUT_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    gpio_set_level(LED_PIN, LED_OFF);
}

bool board_is_input_high(void)
{
    return gpio_get_level(INPUT_PIN) == 1;
}