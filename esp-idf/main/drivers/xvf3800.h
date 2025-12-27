/**
 * @file xvf3800.h
 * @brief XMOS XVF3800 Voice Processor Driver (ReSpeaker)
 *
 * XVF3800 features:
 *   - 4-microphone array
 *   - Acoustic Echo Cancellation (AEC)
 *   - Noise Suppression
 *   - Automatic Gain Control
 *   - Voice Activity Detection
 *   - Beamforming
 *
 * Connection: I2S1 (RX) for processed audio output
 *
 * Note: XVF3800 handles all DSP internally, ESP32 receives
 *       clean, processed mono audio for voice recognition.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2s_std.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief XVF3800 configuration
 */
typedef struct {
    int bclk_pin;
    int ws_pin;
    int din_pin;
    uint32_t sample_rate;           // Typically 16000Hz for voice
    uint8_t bits_per_sample;        // Typically 16-bit
} xvf3800_config_t;

#define XVF3800_DEFAULT_CONFIG() { \
    .bclk_pin = 45, \
    .ws_pin = 46, \
    .din_pin = 47, \
    .sample_rate = 16000, \
    .bits_per_sample = 16, \
}

/**
 * @brief Audio data callback
 */
typedef void (*xvf3800_audio_cb_t)(const int16_t *samples, size_t num_samples, void *user_data);

esp_err_t xvf3800_init(const xvf3800_config_t *config);
esp_err_t xvf3800_deinit(void);
esp_err_t xvf3800_start(void);
esp_err_t xvf3800_stop(void);
esp_err_t xvf3800_read(void *data, size_t len, size_t *bytes_read, uint32_t timeout_ms);
esp_err_t xvf3800_register_callback(xvf3800_audio_cb_t callback, void *user_data);
i2s_chan_handle_t xvf3800_get_i2s_handle(void);
bool xvf3800_is_initialized(void);

#ifdef __cplusplus
}
#endif
