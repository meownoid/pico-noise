#include "pico/stdlib.h"
#include "pico/audio_i2s.h"
#include "hardware/adc.h"

#define SAMPLE_RATE 96000
#define SAMPLES_PER_BUFFER 512
#define BUFFER_COUNT 4
#define LOWPASS_CUTOFF 100

#define PICO_AUDIO_PACK_I2S_DATA 9
#define PICO_AUDIO_PACK_I2S_BCLK 10

// Function to collect entropy from ADC noise
uint8_t adc_random_bit()
{
    adc_select_input(0);
    uint16_t raw_adc_value = adc_read();
    return raw_adc_value & 1;
}

uint64_t adc_random_number()
{
    uint64_t result = 0;
    for (int i = 0; i < 64; i++)
    {
        result = (result << 1) | adc_random_bit();
    }
    return result;
}

/*
 * PCG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *       http://www.pcg-random.org
 */

typedef struct
{
    uint64_t state;
    uint64_t inc;
} pcg32_random_t;

static pcg32_random_t pcg32_global = {0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL};

inline uint32_t pcg32_random_r(pcg32_random_t *rng)
{
    uint64_t oldstate = rng->state;
    // Advance internal state
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc | 1);
    // Calculate output function (XSH RR), uses old state for max ILP
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

inline void pcg32_srandom_r(pcg32_random_t *rng, uint64_t initstate, uint64_t initseq)
{
    rng->state = 0U;
    rng->inc = (initseq << 1u) | 1u;
    pcg32_random_r(rng);
    rng->state += initstate;
    pcg32_random_r(rng);
}

inline void pcg32_srandom(uint64_t seed, uint64_t seq)
{
    pcg32_srandom_r(&pcg32_global, seed, seq);
}

inline uint32_t pcg32_random()
{
    return pcg32_random_r(&pcg32_global);
}

inline int32_t random_normal()
{
    uint32_t result = 0;
    for (uint i = 0; i < 6; i++)
    {
        uint32_t r = pcg32_random();
        result += (r & 0xffff) + (r >> 16);
    }
    return result / 6 - 0xffff;
}

inline int32_t clip16(int32_t sample)
{
    return sample <= -0x8000 ? -0x8000 : (sample > 0x7fff ? 0x7fff : sample);
}

int main()
{
    stdio_init_all();
    adc_init();
    adc_gpio_init(26); // ADC0
    pcg32_srandom(adc_random_number() ^ time_us_64(), 0);

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
    if (!output_format)
    {
        panic("PicoAudio: Unable to open audio device.\n");
    }

    bool status = audio_i2s_connect_extra(producer_pool, false, BUFFER_COUNT, SAMPLES_PER_BUFFER, NULL);
    if (!status)
    {
        panic("PicoAudio: Unable to connect to audio device.\n");
    }

    audio_i2s_set_enabled(true);

    const int64_t rc = (1LL << 41) / (2 * 3217 * LOWPASS_CUTOFF);
    const int64_t dt = (1LL << 31) / SAMPLE_RATE;
    const int64_t alpha = (1LL << 62) / SAMPLE_RATE / (rc + dt);
    const int64_t alpha_inv = (1LL << 31) - alpha;

    int32_t x = 0;
    int32_t y = 0;

    while (true)
    {
        struct audio_buffer *buffer = take_audio_buffer(producer_pool, true);
        int16_t *samples = (int16_t *)buffer->buffer->bytes;
        for (uint i = 0; i < buffer->max_sample_count; i++)
        {
            // Integrate normal white noise
            x = clip16(x + (random_normal() >> 7));
            // Apply low-pass filter
            y = ((alpha * (int64_t)x) + (alpha_inv * (int64_t)y)) >> 31;
            // Output to audio buffer
            samples[i] = clip16(y);
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(producer_pool, buffer);
    }
}
