#include "pico/audio_i2s.h"

#define SAMPLE_RATE 48000
#define SAMPLES_PER_BUFFER 512

#define PICO_AUDIO_PACK_I2S_DATA 9
#define PICO_AUDIO_PACK_I2S_BCLK 10


int main() {
    static audio_format_t audio_format = {
        .sample_freq = SAMPLE_RATE,
        .format = AUDIO_BUFFER_FORMAT_PCM_S16,
        .channel_count = 1,
    };

    static struct audio_buffer_format producer_format = {
        .format = &audio_format,
        .sample_stride = 2
    };

    struct audio_buffer_pool *producer_pool = audio_new_producer_pool(
        &producer_format,
        3,
        SAMPLES_PER_BUFFER
    );

    struct audio_i2s_config config = {
        .data_pin = PICO_AUDIO_PACK_I2S_DATA,
        .clock_pin_base = PICO_AUDIO_PACK_I2S_BCLK,
        .dma_channel = 0,
        .pio_sm = 0,
    };

    const struct audio_format *output_format = audio_i2s_setup(&audio_format, &config);
    if (!output_format) {
        panic("PicoAudio: Unable to open audio device.\n");
    }

    bool status = audio_i2s_connect(producer_pool);
    if (!status) {
        panic("PicoAudio: Unable to connect to audio device.\n");
    }

    audio_i2s_set_enabled(true);
    
    while(true) {
        struct audio_buffer *buffer = take_audio_buffer(producer_pool, true);
        int16_t *samples = (int16_t *) buffer->buffer->bytes;
        for (uint i = 0; i < buffer->max_sample_count; i++) {
            samples[i] = (int16_t)0;
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(producer_pool, buffer);
    }
}