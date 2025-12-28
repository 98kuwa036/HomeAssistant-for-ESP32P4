/**
 * @file led_effect.h
 * @brief Phantom LED Ring Effect Controller (Nest Mini Style)
 *
 * Controls WS2812B LED ring via RMT peripheral with various effects:
 * - Breathing: Soft pulsing when idle
 * - Listening: Wave effect when voice detected
 * - Thinking: Rotating animation during processing
 * - Speaking: Visualization during TTS output
 * - Error: Flash red on errors
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// LED Effect Modes
// ============================================================================

typedef enum {
    LED_MODE_OFF = 0,           // All LEDs off
    LED_MODE_BREATHING,         // Soft breathing (idle)
    LED_MODE_LISTENING,         // Voice active (wave effect)
    LED_MODE_THINKING,          // Processing (rotating dots)
    LED_MODE_SPEAKING,          // TTS output (visualizer)
    LED_MODE_NOTIFICATION,      // Attention getter
    LED_MODE_SUCCESS,           // Action completed (green flash)
    LED_MODE_ERROR,             // Error indication (red flash)
    LED_MODE_WIFI_CONNECTING,   // WiFi connecting (blue pulse)
    LED_MODE_CUSTOM             // Custom animation
} led_mode_t;

// ============================================================================
// Color Definitions
// ============================================================================

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} led_color_t;

// Predefined colors (Nest Mini palette)
#define LED_COLOR_WHITE     (led_color_t){255, 255, 255}
#define LED_COLOR_WARM      (led_color_t){255, 200, 150}
#define LED_COLOR_BLUE      (led_color_t){0, 120, 255}
#define LED_COLOR_CYAN      (led_color_t){0, 255, 200}
#define LED_COLOR_GREEN     (led_color_t){0, 255, 100}
#define LED_COLOR_ORANGE    (led_color_t){255, 100, 0}
#define LED_COLOR_RED       (led_color_t){255, 0, 0}
#define LED_COLOR_PURPLE    (led_color_t){150, 0, 255}
#define LED_COLOR_OFF       (led_color_t){0, 0, 0}

// ============================================================================
// Configuration
// ============================================================================

typedef struct {
    int gpio_num;               // Data GPIO
    int led_count;              // Number of LEDs
    int rmt_channel;            // RMT channel
    uint8_t brightness;         // Global brightness (0-255)
    led_color_t primary_color;  // Primary color for effects
    led_color_t secondary_color;// Secondary color for effects
} led_effect_config_t;

// Default configuration macro
#define LED_EFFECT_DEFAULT_CONFIG() { \
    .gpio_num = CONFIG_LED_STRIP_GPIO, \
    .led_count = CONFIG_LED_STRIP_COUNT, \
    .rmt_channel = CONFIG_LED_STRIP_RMT_CHANNEL, \
    .brightness = 128, \
    .primary_color = LED_COLOR_WARM, \
    .secondary_color = LED_COLOR_BLUE \
}

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Initialize LED effect controller
 * @return ESP_OK on success
 */
esp_err_t led_effect_init(void);

/**
 * @brief Initialize with custom configuration
 * @param config Configuration structure
 * @return ESP_OK on success
 */
esp_err_t led_effect_init_config(const led_effect_config_t *config);

/**
 * @brief Deinitialize LED effect controller
 */
void led_effect_deinit(void);

/**
 * @brief Update LED animation (call from LED task)
 *
 * This should be called at ~50Hz for smooth animations.
 */
void led_effect_update(void);

/**
 * @brief Set LED effect mode
 * @param mode Effect mode
 */
void led_effect_set_mode(led_mode_t mode);

/**
 * @brief Get current LED effect mode
 * @return Current mode
 */
led_mode_t led_effect_get_mode(void);

/**
 * @brief Set global brightness
 * @param brightness Brightness level (0-255)
 */
void led_effect_set_brightness(uint8_t brightness);

/**
 * @brief Set primary color for effects
 * @param color RGB color
 */
void led_effect_set_primary_color(led_color_t color);

/**
 * @brief Set secondary color for effects
 * @param color RGB color
 */
void led_effect_set_secondary_color(led_color_t color);

/**
 * @brief Flash error indication
 */
void led_effect_flash_error(void);

/**
 * @brief Flash success indication
 */
void led_effect_flash_success(void);

/**
 * @brief Set individual LED color
 * @param index LED index (0 to led_count-1)
 * @param color RGB color
 */
void led_effect_set_pixel(int index, led_color_t color);

/**
 * @brief Set all LEDs to same color
 * @param color RGB color
 */
void led_effect_set_all(led_color_t color);

/**
 * @brief Clear all LEDs (turn off)
 */
void led_effect_clear(void);

/**
 * @brief Force refresh LED strip
 */
void led_effect_refresh(void);

/**
 * @brief Set audio level for visualizer effect
 * @param level Audio level (0.0 - 1.0)
 */
void led_effect_set_audio_level(float level);

#ifdef __cplusplus
}
#endif
