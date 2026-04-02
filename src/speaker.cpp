#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

#include "speaker.h"
#include "board.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"

#define TAG "SPEAKER"

#define I2C_PORT I2C_NUM_0
#define SDA_PIN GPIO_NUM_4
#define SCL_PIN GPIO_NUM_5
#define I2C_FREQ_HZ 400000

#define MCP4725_ADDR 0x60

struct __attribute__((packed)) Twavheader
{
    char chunk_ID[4];
    uint32_t chunk_size;
    char format[4];

    char sub_chunk1_ID[4];
    uint32_t sub_chunk1_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;

    char sub_chunk2_ID[4];
    uint32_t sub_chunk2_size;
};

esp_err_t speaker::i2c_init()
{
    if (i2c_initialized) {
        return ESP_OK;
    }

    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = SDA_PIN;
    conf.scl_io_num = SCL_PIN;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_FREQ_HZ;
    conf.clk_flags = 0;

    esp_err_t err = i2c_param_config(I2C_PORT, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed: %s", esp_err_to_name(err));
        return err;
    }

    err = i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "i2c_driver_install failed: %s", esp_err_to_name(err));
        return err;
    }

    i2c_initialized = true;
    return ESP_OK;
}

esp_err_t speaker::mcp4725_write(uint16_t value)
{
    value &= 0x0FFF;

    uint8_t buf[3];
    buf[0] = 0x40;
    buf[1] = (value >> 4) & 0xFF;
    buf[2] = (value & 0x0F) << 4;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MCP4725_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, buf, sizeof(buf), true);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(20));
    i2c_cmd_link_delete(cmd);

    return err;
}

void speaker::delay_us(uint32_t us)
{
    esp_rom_delay_us(us);
}

esp_err_t speaker::init()
{
    return i2c_init();
}

esp_err_t speaker::play_wav_file(const std::string& fname)
{
    esp_err_t err = i2c_init();
    if (err != ESP_OK) {
        return err;
    }

    std::ifstream wavfile(fname, std::ios::binary);
    if (!wavfile.is_open()) {
        ESP_LOGE(TAG, "FAILED to open file: %s", fname.c_str());
        return ESP_FAIL;
    }

    Twavheader wav{};
    wavfile.read(reinterpret_cast<char*>(&wav), sizeof(Twavheader));

    if (!wavfile || std::string(wav.chunk_ID, 4) != "RIFF" || std::string(wav.format, 4) != "WAVE") {
        ESP_LOGE(TAG, "Not a RIFF/WAVE file");
        return ESP_FAIL;
    }

    if (wav.audio_format != 1) {
        ESP_LOGE(TAG, "Unsupported audio format: %u", wav.audio_format);
        return ESP_FAIL;
    }

    if (wav.num_channels != 1) {
        ESP_LOGE(TAG, "Only mono WAV supported");
        return ESP_FAIL;
    }

    if (wav.sample_rate != 8000) {
        ESP_LOGE(TAG, "Only 8000 Hz WAV supported, got %lu", (unsigned long)wav.sample_rate);
        return ESP_FAIL;
    }

    if (wav.bits_per_sample != 8) {
        ESP_LOGE(TAG, "Only 8-bit WAV supported, got %u", wav.bits_per_sample);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "WAV OK: %u Hz, %u bit, %u channels, data=%lu bytes",
             (unsigned)wav.sample_rate,
             (unsigned)wav.bits_per_sample,
             (unsigned)wav.num_channels,
             (unsigned long)wav.sub_chunk2_size);

    std::vector<uint8_t> audio_data(static_cast<size_t>(wav.sub_chunk2_size));
    wavfile.read(reinterpret_cast<char*>(audio_data.data()),
                 static_cast<std::streamsize>(audio_data.size()));

    if (!wavfile) {
        ESP_LOGE(TAG, "Failed to read WAV data");
        return ESP_FAIL;
    }

    const uint32_t sample_period_us = 1000000 / 8000; // 125 us

    ESP_LOGI(TAG, "Playing audio...");
    int64_t next_tick = esp_timer_get_time();

    for (size_t i = 0; i < audio_data.size(); ++i) {
        if (!board_is_led_on()) {
            ESP_LOGI(TAG, "LED turned off, stopping playback");
            return ESP_ERR_INVALID_STATE;
        }

        uint8_t s = audio_data[i];
        uint16_t dac_value = static_cast<uint16_t>(s) << 4;

        err = mcp4725_write(dac_value);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "MCP4725 write failed at sample %u: %s",
                     (unsigned)i, esp_err_to_name(err));
            return err;
        }

        next_tick += sample_period_us;
        int64_t now = esp_timer_get_time();
        if (next_tick > now) {
            delay_us(static_cast<uint32_t>(next_tick - now));
        } else {
            next_tick = now;
        }
    }

    ESP_LOGI(TAG, "Playback finished");
    return ESP_OK;
}