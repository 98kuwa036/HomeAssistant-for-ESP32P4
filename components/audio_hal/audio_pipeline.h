/**
 * @file audio_pipeline.h
 * @brief Dual I2S Audio Pipeline for Omni-P4
 *
 * Architecture:
 *   ┌─────────────────────────────────────────────────────────────────┐
 *   │                    Audio Pipeline                                │
 *   ├─────────────────────────────────────────────────────────────────┤
 *   │                                                                  │
 *   │   I2S0 (Output - Master)           I2S1 (Input - Slave)        │
 *   │   ┌─────────────────┐              ┌─────────────────┐          │
 *   │   │   ES9039Q2M     │              │  ReSpeaker      │          │
 *   │   │   Hi-Res DAC    │              │  XVF3800        │          │
 *   │   │   32bit/384kHz  │              │  Beamforming    │          │
 *   │   └────────┬────────┘              └────────┬────────┘          │
 *   │            │                                │                   │
 *   │            ▼                                ▼                   │
 *   │   ┌─────────────────┐              ┌─────────────────┐          │
 *   │   │   TPA3116D2     │              │  Voice Buffer   │          │
 *   │   │   Class-D Amp   │              │  (Ring Buffer)  │          │
 *   │   │   2x15W @ 4Ω    │              │                 │          │
 *   │   └─────────────────┘              └─────────────────┘          │
 *   │                                                                  │
 *   └─────────────────────────────────────────────────────────────────┘
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants
// ============================================================================

#define AUDIO_BUFFER_SIZE       4096
#define AUDIO_DMA_DESC_NUM      6
#define AUDIO_DMA_FRAME_NUM     240

// LED notification bits (for voice activity)
#define LED_NOTIFY_VOICE_ACTIVE     BIT0
#define LED_NOTIFY_VOICE_PROCESSING BIT1
#define LED_NOTIFY_VOICE_DONE       BIT2
#define LED_NOTIFY_ERROR            BIT3

// ============================================================================
// Data Structures
// ============================================================================

/**
 * @brief Audio pipeline configuration
 */
typedef struct {
    // I2S0 Output (DAC)
    struct {
        int bck_gpio;
        int ws_gpio;
        int dout_gpio;
        int mclk_gpio;
        uint32_t sample_rate;
        uint8_t bits_per_sample;
    } i2s0_output;

    // I2S1 Input (Mic)
    struct {
        int bck_gpio;
        int ws_gpio;
        int din_gpio;
        int mclk_gpio;
        uint32_t sample_rate;
        uint8_t bits_per_sample;
    } i2s1_input;

    // DAC I2C address (for volume control)
    uint8_t dac_i2c_addr;

} audio_pipeline_config_t;

/**
 * @brief Audio pipeline state
 */
typedef enum {
    AUDIO_STATE_UNINITIALIZED = 0,
    AUDIO_STATE_IDLE,
    AUDIO_STATE_PLAYING,
    AUDIO_STATE_RECORDING,
    AUDIO_STATE_DUPLEX,
    AUDIO_STATE_ERROR
} audio_state_t;

/**
 * @brief Voice activity detection result
 */
typedef struct {
    bool is_active;
    float energy_db;
    uint32_t duration_ms;
} voice_activity_t;

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Initialize audio pipeline with default configuration
 * @return ESP_OK on success
 */
esp_err_t audio_pipeline_init(void);

/**
 * @brief Initialize audio pipeline with custom configuration
 * @param config Custom configuration
 * @return ESP_OK on success
 */
esp_err_t audio_pipeline_init_config(const audio_pipeline_config_t *config);

/**
 * @brief Deinitialize audio pipeline
 */
void audio_pipeline_deinit(void);

/**
 * @brief Wait for audio pipeline to be ready
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK if ready, ESP_ERR_TIMEOUT on timeout
 */
esp_err_t audio_pipeline_wait_ready(uint32_t timeout_ms);

/**
 * @brief Process audio data (called from audio task)
 *
 * This function should be called periodically from the audio task.
 * It handles buffer management, voice detection, and data routing.
 */
void audio_pipeline_process(void);

/**
 * @brief Get current audio state
 */
audio_state_t audio_pipeline_get_state(void);

// --- Playback Control ---

/**
 * @brief Start audio playback
 * @return ESP_OK on success
 */
esp_err_t audio_pipeline_play(void);

/**
 * @brief Stop audio playback
 */
esp_err_t audio_pipeline_stop(void);

/**
 * @brief Pause audio playback
 */
esp_err_t audio_pipeline_pause(void);

/**
 * @brief Resume audio playback
 */
esp_err_t audio_pipeline_resume(void);

/**
 * @brief Write audio data to output buffer
 * @param data Audio data (PCM)
 * @param len Data length in bytes
 * @param timeout_ms Write timeout
 * @return Number of bytes written
 */
size_t audio_pipeline_write(const uint8_t *data, size_t len, uint32_t timeout_ms);

// --- Recording Control ---

/**
 * @brief Start audio recording
 * @return ESP_OK on success
 */
esp_err_t audio_pipeline_record_start(void);

/**
 * @brief Stop audio recording
 */
esp_err_t audio_pipeline_record_stop(void);

/**
 * @brief Read audio data from input buffer
 * @param data Buffer to store audio data
 * @param len Maximum bytes to read
 * @param timeout_ms Read timeout
 * @return Number of bytes read
 */
size_t audio_pipeline_read(uint8_t *data, size_t len, uint32_t timeout_ms);

// --- Voice Activity ---

/**
 * @brief Check if voice is currently detected
 * @return true if voice detected
 */
bool audio_pipeline_voice_detected(void);

/**
 * @brief Get voice activity details
 * @param activity Pointer to activity structure
 */
void audio_pipeline_get_voice_activity(voice_activity_t *activity);

// --- Volume Control ---

/**
 * @brief Set master volume
 * @param volume Volume level (0-100)
 * @return ESP_OK on success
 */
esp_err_t audio_pipeline_set_volume(uint8_t volume);

/**
 * @brief Get current volume
 * @return Current volume (0-100)
 */
uint8_t audio_pipeline_get_volume(void);

/**
 * @brief Mute/unmute audio output
 * @param mute true to mute
 */
esp_err_t audio_pipeline_set_mute(bool mute);

// --- Diagnostics ---

/**
 * @brief Get audio buffer fill level
 * @param output_level Output buffer fill percentage
 * @param input_level Input buffer fill percentage
 */
void audio_pipeline_get_buffer_levels(uint8_t *output_level, uint8_t *input_level);

/**
 * @brief Get audio statistics
 * @param underruns Number of buffer underruns
 * @param overruns Number of buffer overruns
 */
void audio_pipeline_get_stats(uint32_t *underruns, uint32_t *overruns);

#ifdef __cplusplus
}
#endif
