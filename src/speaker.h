#pragma once

#include <string>
#include <cstdint>
#include "esp_err.h"

class speaker
{
public:
    esp_err_t init();
    esp_err_t playWavFile(const std::string& fname);

private:
    esp_err_t i2c_init();
    esp_err_t dacWrite(uint16_t value);
    void delay_us(uint32_t us);

    bool i2c_initialized = false;
};