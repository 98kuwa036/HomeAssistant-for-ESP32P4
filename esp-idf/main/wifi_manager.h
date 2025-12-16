/**
 * WiFi Manager for ESP32-P4 (via ESP32-C6)
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize WiFi manager
 * Connects to configured WiFi network
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_init(void);

/**
 * Check if WiFi is connected
 *
 * @return true if connected
 */
bool wifi_manager_is_connected(void);

/**
 * Get current IP address as string
 *
 * @return IP address string or NULL if not connected
 */
const char* wifi_manager_get_ip(void);

#ifdef __cplusplus
}
#endif
