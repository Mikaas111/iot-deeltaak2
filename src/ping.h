#pragma once

#include <cstdint>
#include <functional>
#include "driver/gpio.h"
#include "esp_err.h"

class ping {
public:
    using presence_callback_t = std::function<void(bool present)>;

    ping(gpio_num_t trig_pin,
         gpio_num_t echo_pin,
         float threshold_cm = 20.0f,
         uint32_t hold_ms_after_lost = 5000);

    esp_err_t init();
    void update();

    void set_presence_callback(presence_callback_t cb);
    void set_enabled(bool enabled);
    bool is_enabled() const;

private:
    enum class State {
        IDLE,
        WAIT_FOR_ECHO
    };

    gpio_num_t trig_pin_;
    gpio_num_t echo_pin_;
    float threshold_cm_;
    uint32_t hold_ms_after_lost_;

    State state_;
    int64_t last_measure_us_;
    int64_t trig_start_us_;

    bool enabled_;
    bool object_present_;
    int64_t above_threshold_since_us_;

    volatile int64_t echo_rise_us_;
    volatile int64_t echo_fall_us_;
    volatile bool echo_done_;

    presence_callback_t presence_callback_;

    void start_measurement();
    void finish_measurement(int64_t pulse_us);
    void set_presence(bool present);

    static void echo_isr_handler(void *arg);
    void handle_echo_edge();
};