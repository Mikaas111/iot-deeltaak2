#ifndef _BOARD_H_
#define _BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif /**< __cplusplus */

#include "driver/gpio.h"

#ifndef LED_PIN
#define LED_PIN 2
#endif

#define LED_ON  1
#define LED_OFF 0

#define INPUT_PIN 14

    bool board_is_input_high(void);
    void board_init(void);
    void board_set_led(uint8_t onoff);

    struct _led_state {
        uint8_t current;
        uint8_t previous;
        uint8_t pin;
        char *name;
    };

    void board_led_operation(uint8_t pin, uint8_t onoff);

#ifdef __cplusplus
}
#endif /**< __cplusplus */

#endif /* _BOARD_H_ */