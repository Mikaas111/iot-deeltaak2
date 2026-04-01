#include <iostream>
#include <fstream>
#include <vector>
#include "speaker.h"

#define SDA_PIN GPIO_NUM_21
#define SCL_PIN GPIO_NUM_22

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

        if(wavfile.is_open())
        {
            // Read the WAV header
            Twavheader wav;
            wavfile.read(reinterpret_cast<char*>(&wav), sizeof(Twavheader));

            // If the file is a valid WAV file
            if (std::string(wav.format, 4) != "WAVE" || std::string(wav.chunk_ID, 4) != "RIFF")
            {
                wavfile.close();
                std::cerr << "Not a WAVE or RIFF!" << std::endl;
                return;
            }

            // Properties of WAV File
            std::cout << "FileName:" << fname << std::endl;
            std::cout << "File size:" << wav.chunk_size+8 << std::endl;
            std::cout << "Resource Exchange File Mark:" << std::string(wav.chunk_ID, 4) << std::endl;
            std::cout << "Format:" << std::string(wav.format, 4) << std::endl;
            std::cout << "Channels: " << wav.num_channels << std::endl;
            std::cout << "Sample Rate: " << wav.sample_rate << " Hz" << std::endl;
            std::cout << "Bits Per Sample: " << wav.bits_per_sample << " bits" << std::endl;

            // Read wave data
            std::vector<int16_t> audio_data( wav.sub_chunk2_size / sizeof(int16_t) );
            wavfile.read(reinterpret_cast<char*>( audio_data.data() ), wav.sub_chunk2_size );
            wavfile.close();  // Close audio file

            // Display some audio samples
            const size_t numofsample = 20;
            std::cout <<"Listing first " << numofsample << " Samples:" << std::endl;
            for (size_t i = 0; i < numofsample && i < audio_data.size(); ++i)
            {
                std::cout << i << ":" << audio_data[i] << std::endl;
            }

            std::cout << std::endl;
        } else
        {
            std::cout << "FAILED to open file: " << fname << std::endl;
        }
    }