#include "ping.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

#define TAG "PING_SENSOR"

static constexpr int64_t TRIGGER_PULSE_US = 10;
static constexpr int64_t ECHO_TIMEOUT_US = 30000;
static constexpr int64_t MEASURE_INTERVAL_US = 100000; // 100 ms

ping::ping(gpio_num_t trig_pin,
           gpio_num_t echo_pin,
           float threshold_cm,
           uint32_t hold_ms_after_lost)
    : trig_pin_(trig_pin),
      echo_pin_(echo_pin),
      threshold_cm_(threshold_cm),
      hold_ms_after_lost_(hold_ms_after_lost),
      state_(State::IDLE),
      last_measure_us_(0),
      trig_start_us_(0),
      enabled_(true),
      object_present_(false),
      above_threshold_since_us_(-1),
      echo_rise_us_(0),
      echo_fall_us_(0),
      echo_done_(false)
{
}

esp_err_t ping::init()
{
    gpio_reset_pin(trig_pin_);
    gpio_set_direction(trig_pin_, GPIO_MODE_OUTPUT);
    gpio_set_level(trig_pin_, 0);

    gpio_reset_pin(echo_pin_);
    gpio_set_direction(echo_pin_, GPIO_MODE_INPUT);
    gpio_set_pull_mode(echo_pin_, GPIO_FLOATING);

    gpio_set_intr_type(echo_pin_, GPIO_INTR_ANYEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(echo_pin_, echo_isr_handler, this);
    gpio_intr_enable(echo_pin_);

    state_ = State::IDLE;
    last_measure_us_ = esp_timer_get_time();
    trig_start_us_ = 0;
    enabled_ = true;
    object_present_ = false;
    above_threshold_since_us_ = -1;
    echo_rise_us_ = 0;
    echo_fall_us_ = 0;
    echo_done_ = false;

    ESP_LOGI(TAG, "Ping sensor initialized: trig=%d echo=%d threshold=%.1f cm",
             (int)trig_pin_, (int)echo_pin_, threshold_cm_);

    return ESP_OK;
}

void ping::set_presence_callback(presence_callback_t cb)
{
    presence_callback_ = std::move(cb);
}

void ping::set_enabled(bool enabled)
{
    enabled_ = enabled;
    if (!enabled_) {
        state_ = State::IDLE;
        echo_done_ = false;
    }
}

bool ping::is_enabled() const
{
    return enabled_;
}

void ping::set_presence(bool present)
{
    // BELANGRIJK:
    // niet alleen bij statusverandering melden, maar altijd bij present=true
    // zodat de 5s timer in main opnieuw gereset wordt zolang er iets dichtbij staat.
    object_present_ = present;

    if (presence_callback_) {
        presence_callback_(present);
    }
}

void ping::start_measurement()
{
    echo_done_ = false;
    echo_rise_us_ = 0;
    echo_fall_us_ = 0;

    gpio_set_level(trig_pin_, 0);
    esp_rom_delay_us(2);
    gpio_set_level(trig_pin_, 1);
    esp_rom_delay_us(TRIGGER_PULSE_US);
    gpio_set_level(trig_pin_, 0);

    trig_start_us_ = esp_timer_get_time();
    state_ = State::WAIT_FOR_ECHO;
}

void ping::finish_measurement(int64_t pulse_us)
{
    if (pulse_us <= 0) {
        last_measure_us_ = esp_timer_get_time();
        state_ = State::IDLE;
        return;
    }

    float distance_cm = pulse_us / 58.0f;

    if (distance_cm <= 0.0f || distance_cm > 400.0f) {
        last_measure_us_ = esp_timer_get_time();
        state_ = State::IDLE;
        return;
    }

    ESP_LOGI(TAG, "Distance: %.2f cm", distance_cm);

    if (distance_cm <= threshold_cm_) {
        // Elke keer opnieuw melden als hij dichtbij is
        set_presence(true);
    }

    last_measure_us_ = esp_timer_get_time();
    state_ = State::IDLE;
}

void ping::echo_isr_handler(void *arg)
{
    auto *self = static_cast<ping *>(arg);
    self->handle_echo_edge();
}

void ping::handle_echo_edge()
{
    int level = gpio_get_level(echo_pin_);
    int64_t now = esp_timer_get_time();

    if (level == 1) {
        echo_rise_us_ = now;
    } else {
        echo_fall_us_ = now;
        echo_done_ = true;
    }
}

void ping::update()
{
    if (!enabled_) {
        return;
    }

    int64_t now = esp_timer_get_time();

    switch (state_) {
    case State::IDLE:
        if ((now - last_measure_us_) >= MEASURE_INTERVAL_US) {
            start_measurement();
        }
        break;

    case State::WAIT_FOR_ECHO:
        if (echo_done_) {
            echo_done_ = false;
            int64_t pulse_us = echo_fall_us_ - echo_rise_us_;
            finish_measurement(pulse_us);
        } else if ((now - trig_start_us_) > ECHO_TIMEOUT_US) {
            last_measure_us_ = now;
            state_ = State::IDLE;
        }
        break;
    }
}