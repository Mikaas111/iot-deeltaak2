#include "button.h"
#include "driver/gpio.h"

button::button(gpio_num_t p, std::function<void()> cb)
    : pin(p), last_state(true), callback(cb)
{
    // Configure GPIO
    gpio_reset_pin(this->pin);
    gpio_set_direction(this->pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(this->pin, GPIO_PULLUP_ONLY);

    // Initialize state correctly (prevents false trigger on boot)
    this->last_state = gpio_get_level(this->pin);
}

void button::poll()
{
    bool current = gpio_get_level(this->pin);

    // Detect falling edge (button press)
    if (this->last_state && !current) {
        if (this->callback) {
            this->callback();
        }
    }

    this->last_state = current;
}