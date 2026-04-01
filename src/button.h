#pragma once
#include <functional>
#include "driver/gpio.h"

class button {
public:
    button(gpio_num_t pin, std::function<void()> callback);
    void poll();  // Call periodically in loop

private:
    gpio_num_t pin;
    bool last_state;
    std::function<void()> callback;
};