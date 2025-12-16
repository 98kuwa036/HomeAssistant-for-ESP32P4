/**
 * Home Assistant Client
 *
 * Communicates with Home Assistant via REST API and WebSocket
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize Home Assistant client
 *
 * @return ESP_OK on success
 */
esp_err_t ha_client_init(void);

/**
 * Deinitialize Home Assistant client
 */
void ha_client_deinit(void);

/**
 * Process Home Assistant messages
 * Should be called periodically from main task
 */
void ha_client_process(void);

/**
 * Send audio data to Home Assistant for STT processing
 *
 * @param audio_data Audio data buffer
 * @param length Length of audio data
 * @return ESP_OK on success
 */
esp_err_t ha_client_send_audio(const uint8_t *audio_data, size_t length);

/**
 * Receive TTS audio from Home Assistant
 *
 * @param buffer Buffer to store audio data
 * @param buffer_size Size of buffer
 * @param bytes_received Actual bytes received
 * @return ESP_OK on success
 */
esp_err_t ha_client_receive_tts(uint8_t *buffer, size_t buffer_size, size_t *bytes_received);

/**
 * Send sensor data to Home Assistant
 *
 * @param entity_id Entity ID (e.g., "sensor.smart_speaker_co2")
 * @param value Sensor value
 * @return ESP_OK on success
 */
esp_err_t ha_client_send_sensor(const char *entity_id, float value);

/**
 * Check if connected to Home Assistant
 *
 * @return true if connected
 */
bool ha_client_is_connected(void);

#ifdef __cplusplus
}
#endif
