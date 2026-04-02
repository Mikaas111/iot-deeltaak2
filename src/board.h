#ifndef BOARD_H
#define BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/gpio.h"
#include <stdbool.h>
#include <stdint.h>

#ifndef LED_PIN
#define LED_PIN 2
#endif

#define LED_ON  1
#define LED_OFF 0

#define INPUT_PIN 14

    struct _led_state {
        uint8_t current;
        uint8_t previous;
        uint8_t pin;
        const char *name;
    };

    bool board_is_input_high(void);
    bool board_is_led_on(void);
    void board_init(void);
    void board_set_led(uint8_t onoff);
    void board_led_operation(uint8_t pin, uint8_t onoff);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */