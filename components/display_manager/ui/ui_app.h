/**
 * @file ui_app.h
 * @brief Advanced UI Application for Omni-P4
 *
 * Features:
 * - 3-panel TileView swipe navigation
 * - Glass Cockpit style dashboard
 * - Nature Remo control center
 * - Standby overlay with digital clock
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// UI Screen Indices
// ============================================================================

typedef enum {
    UI_TILE_CONTROL = 0,    // Left: Control Center (Remo + Tabs)
    UI_TILE_HOME,           // Center: Glass Cockpit Dashboard
    UI_TILE_EXTRA,          // Right: Extra (Future expansion)
    UI_TILE_COUNT
} ui_tile_t;

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Initialize the advanced UI application
 *
 * Creates TileView with 3 screens, glass cockpit dashboard,
 * control center, and standby overlay.
 *
 * @return 0 on success
 */
int ui_app_init(void);

/**
 * @brief Deinitialize UI application
 */
void ui_app_deinit(void);

/**
 * @brief Navigate to specific tile
 *
 * @param tile Target tile index
 */
void ui_app_goto_tile(ui_tile_t tile);

/**
 * @brief Show/hide standby overlay
 *
 * @param show true to show standby screen
 */
void ui_app_set_standby(bool show);

/**
 * @brief Check if standby is active
 *
 * @return true if standby overlay is visible
 */
bool ui_app_is_standby(void);

/**
 * @brief Update time display (standby clock)
 *
 * @param hour Hour (0-23)
 * @param minute Minute (0-59)
 * @param second Second (0-59)
 */
void ui_app_update_time(int hour, int minute, int second);

/**
 * @brief Update glass cockpit sensor data
 *
 * @param temp Temperature in Celsius
 * @param humidity Humidity in %
 * @param co2 CO2 in ppm
 * @param pressure Pressure in hPa
 */
void ui_app_update_sensors(float temp, float humidity, int co2, float pressure);

/**
 * @brief Update audio waveform display
 *
 * @param samples Audio sample data
 * @param count Number of samples
 */
void ui_app_update_waveform(const int16_t *samples, int count);

/**
 * @brief Refresh control center buttons from remo_client
 */
void ui_app_refresh_controls(void);

#ifdef __cplusplus
}
#endif
