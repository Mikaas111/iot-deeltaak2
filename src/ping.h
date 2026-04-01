#pragma once

#include <cstdint>
#include "driver/gpio.h"
#include "esp_err.h"

class ping {
public:
    ping(gpio_num_t trig_pin, gpio_num_t echo_pin, gpio_num_t led_pin, float threshold_cm = 20.0f);

    esp_err_t init();
    void update();

private:
    enum class State {
        IDLE,
        TRIGGER_HIGH,
        WAIT_ECHO_HIGH,
        WAIT_ECHO_LOW
    };

    gpio_num_t trig_pin_;
    gpio_num_t echo_pin_;
    gpio_num_t led_pin_;
    float threshold_cm_;

    State state_;
    int64_t state_start_us_;
    int64_t last_measure_us_;
    float last_distance_cm_;

    void set_led(bool on);
    void start_measurement();
    void finish_measurement(int64_t pulse_us);
};