/**
 * @file display_manager.h
 * @brief Display Manager for Omni-P4 (MIPI-DSI + LVGL)
 *
 * Manages the 7-inch MIPI-DSI LCD with LVGL v9.x
 * Features:
 * - Home Assistant dashboard view
 * - Sensor data visualization
 * - Music visualizer
 * - Touch input handling
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "sensor_hub.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Display Configuration
// ============================================================================

#define DISPLAY_WIDTH   CONFIG_DISPLAY_H_RES
#define DISPLAY_HEIGHT  CONFIG_DISPLAY_V_RES

// ============================================================================
// Screen/View Types
// ============================================================================

typedef enum {
    SCREEN_HOME = 0,        // Main home screen with clock/weather
    SCREEN_SENSORS,         // Sensor data dashboard
    SCREEN_MUSIC,           // Music player with visualizer
    SCREEN_CLIMATE,         // Climate control
    SCREEN_SETTINGS,        // Settings menu
    SCREEN_NOTIFICATION,    // Notification overlay
    SCREEN_COUNT
} screen_type_t;

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Initialize display manager
 *
 * Initializes MIPI-DSI, LVGL, and creates UI screens.
 *
 * @return ESP_OK on success
 */
esp_err_t display_manager_init(void);

/**
 * @brief Deinitialize display manager
 */
void display_manager_deinit(void);

/**
 * @brief Lock LVGL mutex for thread-safe access
 * @param timeout_ms Timeout in milliseconds (-1 for infinite)
 * @return true if lock acquired
 */
bool display_manager_lock(int timeout_ms);

/**
 * @brief Unlock LVGL mutex
 */
void display_manager_unlock(void);

/**
 * @brief Call LVGL timer handler
 *
 * Should be called from display task.
 *
 * @return Suggested delay until next call (in ms)
 */
uint32_t display_manager_timer_handler(void);

/**
 * @brief Switch to a specific screen
 * @param screen Screen type to switch to
 */
void display_manager_set_screen(screen_type_t screen);

/**
 * @brief Get current active screen
 * @return Current screen type
 */
screen_type_t display_manager_get_screen(void);

/**
 * @brief Update sensor data display
 * @param data Sensor data structure
 */
void display_manager_update_sensors(const sensor_data_t *data);

/**
 * @brief Update clock display
 * @param hour Hour (0-23)
 * @param minute Minute (0-59)
 */
void display_manager_update_time(int hour, int minute);

/**
 * @brief Update music player display
 * @param title Track title
 * @param artist Artist name
 * @param progress Playback progress (0.0-1.0)
 * @param is_playing true if playing
 */
void display_manager_update_music(const char *title, const char *artist,
                                   float progress, bool is_playing);

/**
 * @brief Update audio spectrum for visualizer
 * @param spectrum Array of spectrum values (0.0-1.0)
 * @param num_bands Number of frequency bands
 */
void display_manager_update_spectrum(const float *spectrum, int num_bands);

/**
 * @brief Show notification toast
 * @param title Notification title
 * @param message Notification message
 * @param duration_ms Display duration
 */
void display_manager_show_notification(const char *title, const char *message,
                                        uint32_t duration_ms);

/**
 * @brief Set display brightness
 * @param brightness Brightness level (0-100)
 */
void display_manager_set_brightness(uint8_t brightness);

/**
 * @brief Get display brightness
 * @return Current brightness (0-100)
 */
uint8_t display_manager_get_brightness(void);

/**
 * @brief Turn display on/off
 * @param on true to turn on
 */
void display_manager_set_power(bool on);

/**
 * @brief Check if display is initialized
 * @return true if initialized
 */
bool display_manager_is_initialized(void);

#ifdef __cplusplus
}
#endif
