#include <iostream>
#include <fstream>
#include <vector>
#include "speaker.h"
#include "esp_log.h"

#define SDA_PIN GPIO_NUM_21
#define SCL_PIN GPIO_NUM_22
#define TAG "SPEAKER"

    struct Twavheader
    {
        char chunk_ID[4];              //  4  riff_mark[4];
        uint32_t chunk_size;           //  4  file_size;
        char format[4];                //  4  wave_str[4];

        char sub_chunk1_ID[4];         //  4  fmt_str[4];
        uint32_t sub_chunk1_size;      //  4  pcm_bit_num;
        uint16_t audio_format;         //  2  pcm_encode;
        uint16_t num_channels;         //  2  sound_channel;
        uint32_t sample_rate;          //  4  pcm_sample_freq;
        uint32_t byte_rate;            //  4  byte_freq;
        uint16_t block_align;          //  2  block_align;
        uint16_t bits_per_sample;      //  2  sample_bits;

        char sub_chunk2_ID[4];         //  4  data_str[4];
        uint32_t sub_chunk2_size;      //  4  sound_size;
    };                                 // 44  bytes TOTAL

void Speaker::read_wav_file(std::string fname)
{
    // Open the WAV file
    std::ifstream wavfile(fname, std::ios::binary);

    //check if the file can be opened
    if (!wavfile.is_open())
    {
        ESP_LOGE(TAG, "FAILED to open file: %s", fname.c_str());
        return;
    }

    // Read the WAV header
    Twavheader wav;
    wavfile.read(reinterpret_cast<char*>(&wav), sizeof(Twavheader));

    // Validate the WAV file
    if (std::string(wav.format, 4) != "WAVE" || std::string(wav.chunk_ID, 4) != "RIFF")
    {
        wavfile.close();
        ESP_LOGE(TAG, "Not a WAVE or RIFF file!");
        return;
    }
    // Read wav data
    std::vector<int16_t> audio_data(wav.sub_chunk2_size / sizeof(int16_t));
    wavfile.read(reinterpret_cast<char*>(audio_data.data()), wav.sub_chunk2_size);
    wavfile.close();

    // Display audio samples
    const size_t numofsample = 200;
    ESP_LOGI(TAG, "Listing first %zu samples:", numofsample);
    for (size_t i = 0; i < numofsample && i < audio_data.size(); ++i)
    {
        ESP_LOGI(TAG, "%zu: %d", i, audio_data[i]);
    }
}