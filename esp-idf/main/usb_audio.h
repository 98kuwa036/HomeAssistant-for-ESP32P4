/**
 * USB Audio (UAC) Handler for ReSpeaker USB Mic Array
 *
 * ESP32-P4 USB Host driver for USB Audio Class devices
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * USB Audio configuration
 */
typedef struct {
    uint32_t sample_rate;       // Sample rate (e.g., 16000)
    uint8_t channels;           // Number of channels (1 = mono, 2 = stereo)
    uint8_t bits_per_sample;    // Bits per sample (16 or 24)
} usb_audio_config_t;

/**
 * USB Audio status
 */
typedef enum {
    USB_AUDIO_STATE_DISCONNECTED,
    USB_AUDIO_STATE_CONNECTED,
    USB_AUDIO_STATE_STREAMING,
    USB_AUDIO_STATE_ERROR,
} usb_audio_state_t;

/**
 * Initialize USB Audio (UAC) driver
 *
 * @param config USB Audio configuration
 * @return ESP_OK on success
 */
esp_err_t usb_audio_init(const usb_audio_config_t *config);

/**
 * Deinitialize USB Audio driver
 */
void usb_audio_deinit(void);

/**
 * Read audio data from USB microphone
 *
 * @param buffer Buffer to store audio data
 * @param buffer_size Size of buffer
 * @param bytes_read Actual bytes read
 * @return ESP_OK on success
 */
esp_err_t usb_audio_read(uint8_t *buffer, size_t buffer_size, size_t *bytes_read);

/**
 * Get current USB Audio state
 *
 * @return Current state
 */
usb_audio_state_t usb_audio_get_state(void);

/**
 * Start audio streaming
 *
 * @return ESP_OK on success
 */
esp_err_t usb_audio_start_stream(void);

/**
 * Stop audio streaming
 *
 * @return ESP_OK on success
 */
esp_err_t usb_audio_stop_stream(void);

#ifdef __cplusplus
}
#endif
