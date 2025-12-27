/**
 * @file es9039.h
 * @brief ESS ES9039Q2M DAC Driver
 *
 * ES9039Q2M features:
 *   - High-performance stereo DAC
 *   - 32-bit audio support
 *   - THD+N: -120dB
 *   - Dynamic range: 125dB
 *   - I2S/DSD input
 *
 * Connection: I2S0 (TX) for audio data
 * Control: I2C for configuration (optional)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2s_std.h"

#ifdef __cplusplus
extern "C" {
#endif

// Sample rates
typedef enum {
    ES9039_SAMPLE_RATE_44100 = 44100,
    ES9039_SAMPLE_RATE_48000 = 48000,
    ES9039_SAMPLE_RATE_88200 = 88200,
    ES9039_SAMPLE_RATE_96000 = 96000,
    ES9039_SAMPLE_RATE_176400 = 176400,
    ES9039_SAMPLE_RATE_192000 = 192000,
} es9039_sample_rate_t;

// Bit depths
typedef enum {
    ES9039_BITS_16 = 16,
    ES9039_BITS_24 = 24,
    ES9039_BITS_32 = 32,
} es9039_bits_t;

/**
 * @brief ES9039 configuration
 */
typedef struct {
    // I2S configuration
    int mclk_pin;
    int bclk_pin;
    int ws_pin;
    int dout_pin;
    es9039_sample_rate_t sample_rate;
    es9039_bits_t bits_per_sample;

    // Optional I2C control
    bool use_i2c_control;
    int i2c_sda_pin;
    int i2c_scl_pin;
} es9039_config_t;

#define ES9039_DEFAULT_CONFIG() { \
    .mclk_pin = 13, \
    .bclk_pin = 12, \
    .ws_pin = 10, \
    .dout_pin = 9, \
    .sample_rate = ES9039_SAMPLE_RATE_48000, \
    .bits_per_sample = ES9039_BITS_16, \
    .use_i2c_control = false, \
    .i2c_sda_pin = -1, \
    .i2c_scl_pin = -1, \
}

esp_err_t es9039_init(const es9039_config_t *config);
esp_err_t es9039_deinit(void);
esp_err_t es9039_write(const void *data, size_t len, size_t *bytes_written);
esp_err_t es9039_set_sample_rate(es9039_sample_rate_t rate);
esp_err_t es9039_set_volume(uint8_t volume);
esp_err_t es9039_mute(bool mute);
i2s_chan_handle_t es9039_get_i2s_handle(void);
bool es9039_is_initialized(void);

#ifdef __cplusplus
}
#endif
