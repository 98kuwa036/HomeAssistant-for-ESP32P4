/**
 * @file usb_audio_input.h
 * @brief USB Audio Input driver using usb_host_uac component
 *
 * This component provides USB Host Audio Class (UAC 1.0) support for ESP32-P4.
 * It wraps the espressif/usb_host_uac component for easy integration with
 * USB microphones like ReSpeaker USB Mic Array.
 *
 * Supported Devices:
 *   - ReSpeaker USB Mic Array v2.0 (VID: 0x2886, PID: 0x0018)
 *   - Any UAC 1.0 compatible USB microphone
 *
 * Audio Format:
 *   - Sample Rate: 16kHz - 48kHz (device dependent)
 *   - Bit Depth: 16-bit
 *   - Channels: Mono/Stereo (device dependent)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// ReSpeaker USB Mic Array v2.0 identifiers
// ============================================================================
#define RESPEAKER_VID           0x2886
#define RESPEAKER_PID           0x0018

// ============================================================================
// Data Types
// ============================================================================

/**
 * @brief USB Audio Input state
 */
typedef enum {
    USB_AUDIO_INPUT_STATE_IDLE = 0,     // Not initialized
    USB_AUDIO_INPUT_STATE_WAITING,      // Waiting for device connection
    USB_AUDIO_INPUT_STATE_CONNECTED,    // Device connected, not streaming
    USB_AUDIO_INPUT_STATE_STREAMING,    // Actively receiving audio
    USB_AUDIO_INPUT_STATE_ERROR         // Error state
} usb_audio_input_state_t;

/**
 * @brief USB Audio device information
 */
typedef struct {
    uint16_t vid;                       // Vendor ID
    uint16_t pid;                       // Product ID
    uint32_t sample_rate;               // Current sample rate (Hz)
    uint8_t channels;                   // Number of channels
    uint8_t bit_depth;                  // Bits per sample
    bool is_respeaker;                  // True if ReSpeaker device
} usb_audio_input_info_t;

/**
 * @brief Audio data callback function type
 *
 * @param data Pointer to audio data (16-bit PCM samples)
 * @param len Length of data in bytes
 * @param user_ctx User context pointer
 */
typedef void (*usb_audio_input_data_cb_t)(const uint8_t *data, size_t len, void *user_ctx);

/**
 * @brief Device connection callback function type
 *
 * @param connected true if connected, false if disconnected
 * @param info Device information (valid only when connected)
 * @param user_ctx User context pointer
 */
typedef void (*usb_audio_input_connect_cb_t)(bool connected, const usb_audio_input_info_t *info, void *user_ctx);

/**
 * @brief USB Audio Input configuration
 */
typedef struct {
    usb_audio_input_data_cb_t data_cb;      // Audio data callback (required)
    usb_audio_input_connect_cb_t connect_cb; // Connection status callback (optional)
    void *user_ctx;                          // User context for callbacks
    uint32_t preferred_sample_rate;          // Preferred sample rate (0 = auto)
    uint8_t preferred_channels;              // Preferred channels (0 = auto)
} usb_audio_input_config_t;

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Initialize USB Audio Input
 *
 * Initializes USB Host stack and starts device enumeration.
 * Must be called before any other USB Audio Input functions.
 *
 * @param config Configuration structure
 * @return ESP_OK on success
 */
esp_err_t usb_audio_input_init(const usb_audio_input_config_t *config);

/**
 * @brief Deinitialize USB Audio Input
 *
 * Stops streaming, disconnects device, and releases resources.
 */
void usb_audio_input_deinit(void);

/**
 * @brief Start audio streaming
 *
 * Begins receiving audio data from connected USB microphone.
 * Device must be connected before calling this function.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if no device connected
 */
esp_err_t usb_audio_input_start(void);

/**
 * @brief Stop audio streaming
 *
 * @return ESP_OK on success
 */
esp_err_t usb_audio_input_stop(void);

/**
 * @brief Get current state
 *
 * @return Current USB audio input state
 */
usb_audio_input_state_t usb_audio_input_get_state(void);

/**
 * @brief Get connected device information
 *
 * @param info Pointer to store device info
 * @return ESP_OK if device is connected, ESP_ERR_INVALID_STATE otherwise
 */
esp_err_t usb_audio_input_get_info(usb_audio_input_info_t *info);

/**
 * @brief Check if USB Audio Input is ready
 *
 * @return true if device is connected and streaming
 */
bool usb_audio_input_is_ready(void);

/**
 * @brief Check if connected device is ReSpeaker
 *
 * @return true if ReSpeaker USB Mic Array is connected
 */
bool usb_audio_input_is_respeaker(void);

#ifdef __cplusplus
}
#endif
