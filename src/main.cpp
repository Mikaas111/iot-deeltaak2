#include "speaker.h"
#include "esp_spiffs.h"

void init_spiffs()
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_vfs_spiffs_register(&conf);
}

extern "C" void app_main(void)
{
    init_spiffs();

    Speaker speaker;
    speaker.read_wav_file("/spiffs/brot15.wav");

}
