#include "pico/audio_i2s.h"
#include "pico/stdlib.h"

#define SAMPLE_RATE 48000
#define SAMPLES_PER_BUFFER 512
#define BUFFER_COUNT 4

#define PICO_AUDIO_PACK_I2S_DATA 9
#define PICO_AUDIO_PACK_I2S_BCLK 10


uint32_t prng_xorshift_state = 0x32B71700;

uint32_t prng_xorshift_next() {
    uint32_t x = prng_xorshift_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    prng_xorshift_state = x;
    return x;
}

int32_t prng_normal() {
    uint32_t r0 = prng_xorshift_next();
    uint32_t r1 = prng_xorshift_next();
    uint32_t n = ((r0 & 0xffff) + (r1 & 0xffff) + (r0 >> 16) + (r1 >> 16)) / 2;
    return n - 0xffff;
}


int main() {
    stdio_init_all();

    audio_format_t audio_format = {
        .sample_freq = SAMPLE_RATE,
        .format = AUDIO_BUFFER_FORMAT_PCM_S16,
        .channel_count = 1,
    };

    struct audio_buffer_format producer_format = {
        .format = &audio_format,
        .sample_stride = 2
    };

    struct audio_buffer_pool *producer_pool = audio_new_producer_pool(
        &producer_format,
        BUFFER_COUNT,
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

    int32_t sample = 0;
    
    while(true) {
        struct audio_buffer *buffer = take_audio_buffer(producer_pool, true);
        int16_t *samples = (int16_t *) buffer->buffer->bytes;
        for (uint i = 0; i < buffer->max_sample_count; i++) {
            sample += prng_normal() / 1000;
            samples[i] = static_cast<int16_t>(sample <= -0x8000 ? -0x8000 : (sample > 0x7fff ? 0x7fff : sample));
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(producer_pool, buffer);
    }
}