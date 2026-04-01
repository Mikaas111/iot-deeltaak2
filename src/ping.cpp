#include "ping.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

#define TAG "PING_SENSOR"

static constexpr int64_t TRIGGER_PULSE_US = 10;
static constexpr int64_t ECHO_TIMEOUT_US   = 30000;
static constexpr int64_t MEASURE_INTERVAL_US = 100000; // 100 ms

ping::ping(gpio_num_t trig_pin, gpio_num_t echo_pin, gpio_num_t led_pin, float threshold_cm)
    : trig_pin_(trig_pin),
      echo_pin_(echo_pin),
      led_pin_(led_pin),
      threshold_cm_(threshold_cm),
      state_(State::IDLE),
      state_start_us_(0),
      last_measure_us_(0),
      last_distance_cm_(-1.0f)
{
}

esp_err_t ping::init()
{
    gpio_reset_pin(trig_pin_);
    gpio_set_direction(trig_pin_, GPIO_MODE_OUTPUT);
    gpio_set_level(trig_pin_, 0);

    gpio_reset_pin(echo_pin_);
    gpio_set_direction(echo_pin_, GPIO_MODE_INPUT);

    gpio_reset_pin(led_pin_);
    gpio_set_direction(led_pin_, GPIO_MODE_OUTPUT);
    gpio_set_level(led_pin_, 0);

    state_ = State::IDLE;
    state_start_us_ = esp_timer_get_time();
    last_measure_us_ = 0;
    last_distance_cm_ = -1.0f;

    return ESP_OK;
}

void ping::set_led(bool on)
{
    gpio_set_level(led_pin_, on ? 1 : 0);
}

void ping::start_measurement()
{
    gpio_set_level(trig_pin_, 0);
    esp_rom_delay_us(2);

    gpio_set_level(trig_pin_, 1);
    state_ = State::TRIGGER_HIGH;
    state_start_us_ = esp_timer_get_time();
}

void ping::finish_measurement(int64_t pulse_us)
{
    float distance_cm = pulse_us / 58.0f;
    last_distance_cm_ = distance_cm;

    ESP_LOGI(TAG, "Distance: %.2f cm", distance_cm);

    if (distance_cm > 0 && distance_cm <= threshold_cm_) {
        set_led(true);
    } else {
        set_led(false);
    }

    last_measure_us_ = esp_timer_get_time();
    state_ = State::IDLE;
}

void ping::update()
{
    int64_t now = esp_timer_get_time();

    switch (state_) {
    case State::IDLE:
        if ((now - last_measure_us_) >= MEASURE_INTERVAL_US) {
            start_measurement();
        }
        break;

    case State::TRIGGER_HIGH:
        if ((now - state_start_us_) >= TRIGGER_PULSE_US) {
            gpio_set_level(trig_pin_, 0);
            state_ = State::WAIT_ECHO_HIGH;
            state_start_us_ = now;
        }
        break;

    case State::WAIT_ECHO_HIGH:
        if (gpio_get_level(echo_pin_) == 1) {
            state_ = State::WAIT_ECHO_LOW;
            state_start_us_ = now;
        } else if ((now - state_start_us_) > ECHO_TIMEOUT_US) {
            ESP_LOGW(TAG, "Timeout waiting for echo HIGH");
            set_led(false);
            last_distance_cm_ = -1.0f;
            last_measure_us_ = now;
            state_ = State::IDLE;
        }
        break;

    case State::WAIT_ECHO_LOW:
        if (gpio_get_level(echo_pin_) == 0) {
            int64_t pulse_us = now - state_start_us_;
            finish_measurement(pulse_us);
        } else if ((now - state_start_us_) > ECHO_TIMEOUT_US) {
            ESP_LOGW(TAG, "Timeout waiting for echo LOW");
            set_led(false);
            last_distance_cm_ = -1.0f;
            last_measure_us_ = now;
            state_ = State::IDLE;
        }
        break;
    }
}