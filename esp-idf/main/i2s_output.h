/**
 * I2S Output Handler for ES8311 DAC
 *
 * Handles audio output via ES8311 → PAM8403 → Speakers
 */

#pragma once

#include "esp_err.h"
#include "driver/gpio.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * I2S Output configuration
 */
typedef struct {
    gpio_num_t mclk_pin;        // Master clock pin
    gpio_num_t bclk_pin;        // Bit clock pin
    gpio_num_t ws_pin;          // Word select (LRCLK) pin
    gpio_num_t dout_pin;        // Data out pin
    uint32_t sample_rate;       // Sample rate (e.g., 16000, 44100)
    uint8_t bits_per_sample;    // Bits per sample (16 or 32)
} i2s_output_config_t;

/**
 * Initialize I2S output
 *
 * @param config I2S configuration
 * @return ESP_OK on success
 */
esp_err_t i2s_output_init(const i2s_output_config_t *config);

/**
 * Deinitialize I2S output
 */
void i2s_output_deinit(void);

/**
 * Write audio data to I2S output
 *
 * @param data Audio data buffer
 * @param length Length of data in bytes
 * @param bytes_written Actual bytes written
 * @return ESP_OK on success
 */
esp_err_t i2s_output_write(const uint8_t *data, size_t length, size_t *bytes_written);

/**
 * Set output volume (0-100)
 *
 * @param volume Volume level (0-100)
 * @return ESP_OK on success
 */
esp_err_t i2s_output_set_volume(uint8_t volume);

/**
 * Mute/unmute output
 *
 * @param mute true to mute, false to unmute
 * @return ESP_OK on success
 */
esp_err_t i2s_output_set_mute(bool mute);

#ifdef __cplusplus
}
#endif
