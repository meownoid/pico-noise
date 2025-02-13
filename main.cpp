#include "pico/stdlib.h"
#include "pico/audio_i2s.h"
#include "hardware/adc.h"

#define SAMPLE_RATE 96000
#define SAMPLES_PER_BUFFER 512
#define BUFFER_COUNT 4
#define LOWPASS_CUTOFF 4000

#define PICO_AUDIO_PACK_I2S_DATA 9
#define PICO_AUDIO_PACK_I2S_BCLK 10

#define Q31_ONE (1 << 31)

// Function to collect entropy from ADC noise
uint8_t get_adc_random_bit(void)
{
    adc_select_input(0);
    uint16_t raw_adc_value = adc_read();
    // Return only the least significant bit
    return raw_adc_value & 1;
}

// Function to generate an 8-bit random number
uint8_t get_random_byte(void)
{
    uint8_t random_byte = 0;
    for (int i = 0; i < 8; i++)
    {
        random_byte = (random_byte << 1) | get_adc_random_bit();
    }
    return random_byte;
}

// Function to generate a 64-bit random number
uint64_t get_random_number(void)
{
    uint64_t random_number = 0;
    for (int i = 0; i < 8; i++)
    {
        random_number = (random_number << 8) | get_random_byte();
    }
    return random_number;
}

// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)

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

inline int32_t pcg32_normal()
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
    pcg32_srandom(get_random_number() ^ time_us_64(), 0);

    audio_format_t audio_format = {
        .sample_freq = SAMPLE_RATE,
        .format = AUDIO_BUFFER_FORMAT_PCM_S16,
        .channel_count = 1,
    };

    struct audio_buffer_format producer_format = {
        .format = &audio_format,
        .sample_stride = 2};

    struct audio_buffer_pool *producer_pool = audio_new_producer_pool(
        &producer_format,
        BUFFER_COUNT,
        SAMPLES_PER_BUFFER);

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

    const int64_t RC_q31_lp = (1LL << 31) / (2 * 314 * LOWPASS_CUTOFF);
    const int64_t dt_q31_lp = (1LL << 31) / SAMPLE_RATE;
    const int32_t alpha_lp = (dt_q31_lp * Q31_ONE) / (RC_q31_lp + dt_q31_lp);
    const int32_t alpha_lp_inv = Q31_ONE - alpha_lp;

    int32_t x = 0;
    int32_t y_lp = 0;

    while (true)
    {
        struct audio_buffer *buffer = take_audio_buffer(producer_pool, true);
        int16_t *samples = (int16_t *)buffer->buffer->bytes;
        for (uint i = 0; i < buffer->max_sample_count; i++)
        {
            // Integrate normal white noise
            x = clip16(x + (pcg32_normal() >> 7));
            // Apply low-pass filter
            y_lp = (((int64_t)alpha_lp * (int64_t)x) >> 31) + (((int64_t)alpha_lp_inv * (int64_t)y_lp) >> 31);
            // Output to audio buffer
            samples[i] = clip16(y_lp);
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(producer_pool, buffer);
    }
}