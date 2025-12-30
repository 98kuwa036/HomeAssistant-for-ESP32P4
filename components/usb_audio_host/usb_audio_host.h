/**
 * @file usb_audio_host.h
 * @brief USB Audio Class (UAC) Host Driver for ReSpeaker USB Mic Array
 *
 * This component provides USB Host functionality to receive audio from
 * USB Audio Class devices like ReSpeaker USB Mic Array v2.0.
 *
 * Architecture:
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │                    USB Audio Host                           │
 *   ├─────────────────────────────────────────────────────────────┤
 *   │                                                             │
 *   │   USB Host Stack                                            │
 *   │   ├── USB Host Library                                      │
 *   │   ├── USB Audio Class Driver (UAC 1.0)                      │
 *   │   └── Isochronous Transfer Handler                          │
 *   │                                                             │
 *   │   ReSpeaker USB Mic Array v2.0                              │
 *   │   ├── VID: 0x2886  PID: 0x0018                              │
 *   │   ├── Audio Format: 48kHz, 16-bit, Stereo                   │
 *   │   └── Beamforming + Noise Suppression (XVF3800)             │
 *   │                                                             │
 *   └─────────────────────────────────────────────────────────────┘
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants
// ============================================================================

// ReSpeaker USB Mic Array v2.0 identifiers
#define RESPEAKER_VID           0x2886
#define RESPEAKER_PID           0x0018

// Audio format (fixed for ReSpeaker USB)
#define USB_AUDIO_SAMPLE_RATE   48000
#define USB_AUDIO_CHANNELS      2       // Stereo
#define USB_AUDIO_BIT_DEPTH     16
#define USB_AUDIO_BYTES_PER_SAMPLE  (USB_AUDIO_BIT_DEPTH / 8)
#define USB_AUDIO_FRAME_SIZE    (USB_AUDIO_CHANNELS * USB_AUDIO_BYTES_PER_SAMPLE)

// Buffer sizes
#define USB_AUDIO_PACKET_SIZE   192     // 1ms at 48kHz stereo 16-bit
#define USB_AUDIO_RING_BUFFER_SIZE  (USB_AUDIO_PACKET_SIZE * 64)  // ~64ms buffer

// ============================================================================
// Data Types
// ============================================================================

/**
 * @brief USB Audio Host state
 */
typedef enum {
    USB_AUDIO_STATE_IDLE = 0,       // Not initialized
    USB_AUDIO_STATE_WAITING,        // Waiting for device connection
    USB_AUDIO_STATE_CONNECTED,      // Device connected, not streaming
    USB_AUDIO_STATE_STREAMING,      // Actively receiving audio
    USB_AUDIO_STATE_ERROR           // Error state
} usb_audio_state_t;

/**
 * @brief USB Audio device information
 */
typedef struct {
    uint16_t vid;
    uint16_t pid;
    char manufacturer[64];
    char product[64];
    char serial[64];
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bit_depth;
} usb_audio_device_info_t;

/**
 * @brief Audio data callback function type
 *
 * @param data Pointer to audio data (16-bit PCM samples)
 * @param len Length of data in bytes
 * @param user_ctx User context pointer
 */
typedef void (*usb_audio_data_cb_t)(const uint8_t *data, size_t len, void *user_ctx);

/**
 * @brief Device connection callback function type
 *
 * @param connected true if connected, false if disconnected
 * @param info Device information (valid only when connected)
 * @param user_ctx User context pointer
 */
typedef void (*usb_audio_connect_cb_t)(bool connected, const usb_audio_device_info_t *info, void *user_ctx);

/**
 * @brief USB Audio Host configuration
 */
typedef struct {
    int vbus_gpio;                  // GPIO for VBUS power control (-1 to disable)
    int vbus_active_level;          // Active level for VBUS (0 or 1)
    usb_audio_data_cb_t data_cb;    // Audio data callback
    usb_audio_connect_cb_t connect_cb;  // Connection status callback
    void *user_ctx;                 // User context for callbacks
} usb_audio_host_config_t;

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Initialize USB Audio Host
 *
 * Initializes USB Host stack and starts device enumeration.
 *
 * @param config Configuration structure
 * @return ESP_OK on success
 */
esp_err_t usb_audio_host_init(const usb_audio_host_config_t *config);

/**
 * @brief Deinitialize USB Audio Host
 */
void usb_audio_host_deinit(void);

/**
 * @brief Start audio streaming
 *
 * Must be called after device is connected.
 *
 * @return ESP_OK on success
 */
esp_err_t usb_audio_host_start(void);

/**
 * @brief Stop audio streaming
 *
 * @return ESP_OK on success
 */
esp_err_t usb_audio_host_stop(void);

/**
 * @brief Get current state
 *
 * @return Current USB audio state
 */
usb_audio_state_t usb_audio_host_get_state(void);

/**
 * @brief Get device information
 *
 * @param info Pointer to store device info
 * @return ESP_OK if device is connected
 */
esp_err_t usb_audio_host_get_device_info(usb_audio_device_info_t *info);

/**
 * @brief Read audio data (blocking)
 *
 * Reads audio data from internal ring buffer.
 * Alternative to using callback.
 *
 * @param data Buffer to store audio data
 * @param len Maximum bytes to read
 * @param timeout_ms Timeout in milliseconds
 * @return Number of bytes read
 */
size_t usb_audio_host_read(uint8_t *data, size_t len, uint32_t timeout_ms);

/**
 * @brief Get available audio data length
 *
 * @return Number of bytes available in ring buffer
 */
size_t usb_audio_host_available(void);

/**
 * @brief Check if USB Audio Host is ready
 *
 * @return true if device is connected and streaming
 */
bool usb_audio_host_is_ready(void);

#ifdef __cplusplus
}
#endif
