/**
 * @file sensor_hub.h
 * @brief Phase A: Multi-Sensor Integration Hub for ESP32-P4 Console
 *
 * This module provides unified access to all environmental and interaction sensors:
 *   - SEN66 (I2C): Air quality (CO2, PM2.5, VOC, NOx, Temperature, Humidity)
 *   - SEN0395 (UART): mmWave radar for presence detection and distance
 *   - VL53L1X (I2C): Time-of-Flight distance sensor for proximity detection
 *   - PAJ7620U2 (I2C): Gesture recognition sensor
 *   - VEML7700 (I2C): Ambient light sensor
 *
 * Architecture:
 *   - Thread-safe data access via mutex protection
 *   - Dual-core task distribution for optimal performance
 *   - Event-driven notifications for gesture and proximity changes
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Event Bit Definitions
// =============================================================================

#define SENSOR_GESTURE_EVENT_BIT    BIT0    // New gesture detected
#define SENSOR_PROXIMITY_EVENT_BIT  BIT1    // Proximity state changed
#define SENSOR_PRESENCE_EVENT_BIT   BIT2    // Presence state changed
#define SENSOR_AIR_QUALITY_BIT      BIT3    // Air quality data updated
#define SENSOR_LIGHT_CHANGED_BIT    BIT4    // Significant light change

// =============================================================================
// Gesture Definitions (PAJ7620U2)
// =============================================================================

typedef enum {
    GESTURE_NONE = 0,
    GESTURE_UP,
    GESTURE_DOWN,
    GESTURE_LEFT,
    GESTURE_RIGHT,
    GESTURE_FORWARD,
    GESTURE_BACKWARD,
    GESTURE_CLOCKWISE,
    GESTURE_COUNTERCLOCKWISE,
    GESTURE_WAVE,
} gesture_type_t;

// =============================================================================
// Air Quality Index Levels
// =============================================================================

typedef enum {
    AQI_GOOD = 0,
    AQI_MODERATE,
    AQI_UNHEALTHY_SENSITIVE,
    AQI_UNHEALTHY,
    AQI_VERY_UNHEALTHY,
    AQI_HAZARDOUS,
} aqi_level_t;

// =============================================================================
// Unified Sensor Data Structure
// =============================================================================

/**
 * @brief Air quality data from SEN66
 */
typedef struct {
    float co2_ppm;              // CO2 concentration (400-5000 ppm)
    float pm1_0;                // PM1.0 concentration (µg/m³)
    float pm2_5;                // PM2.5 concentration (µg/m³)
    float pm4_0;                // PM4.0 concentration (µg/m³)
    float pm10;                 // PM10 concentration (µg/m³)
    float voc_index;            // VOC Index (1-500)
    float nox_index;            // NOx Index (1-500)
    float temperature;          // Temperature (°C)
    float humidity;             // Relative Humidity (%)
    aqi_level_t aqi_level;      // Calculated AQI level
    bool valid;                 // Data validity flag
    uint32_t timestamp_ms;      // Last update timestamp
} air_quality_data_t;

/**
 * @brief Presence and distance data from SEN0395 and VL53L1X
 */
typedef struct {
    // SEN0395 mmWave Radar
    bool is_present;            // Human presence detected
    uint16_t radar_distance_cm; // Distance from radar (0-900 cm)
    uint8_t motion_energy;      // Motion energy level (0-100)
    uint8_t static_energy;      // Static presence energy (0-100)

    // VL53L1X ToF Sensor
    uint16_t tof_distance_mm;   // ToF measured distance (0-4000 mm)
    bool tof_valid;             // ToF measurement validity
    uint8_t tof_signal_rate;    // Signal quality indicator

    // Hybrid detection state
    bool approaching;           // Object/person approaching
    bool leaving;               // Object/person leaving
    uint32_t presence_duration_ms;  // How long presence detected
    uint32_t timestamp_ms;      // Last update timestamp
} presence_data_t;

/**
 * @brief Gesture data from PAJ7620U2
 */
typedef struct {
    gesture_type_t last_gesture;    // Most recent gesture
    uint8_t gesture_count;          // Count of gestures in session
    uint32_t last_gesture_time_ms;  // Timestamp of last gesture
    bool gesture_pending;           // Unprocessed gesture available
} gesture_data_t;

/**
 * @brief Ambient light data from VEML7700
 */
typedef struct {
    uint32_t lux;               // Illuminance in lux (0-120000)
    uint16_t white_level;       // White channel raw value
    bool is_dark;               // Below darkness threshold
    bool is_bright;             // Above brightness threshold
    uint32_t timestamp_ms;      // Last update timestamp
} ambient_light_data_t;

/**
 * @brief Complete room environment data structure
 *
 * This is the "Digital Twin" of the room, aggregating all sensor data
 * into a single coherent view of the environment.
 */
typedef struct {
    air_quality_data_t air;
    presence_data_t presence;
    gesture_data_t gesture;
    ambient_light_data_t light;

    // System status
    struct {
        bool sen66_online;
        bool sen0395_online;
        bool vl53l1x_online;
        bool paj7620_online;
        bool veml7700_online;
    } sensor_status;

    uint32_t system_uptime_ms;
} room_environment_t;

// =============================================================================
// Configuration Structures
// =============================================================================

/**
 * @brief Sensor hub configuration
 */
typedef struct {
    // I2C Configuration
    int i2c_sda_pin;
    int i2c_scl_pin;
    uint32_t i2c_freq_hz;

    // UART Configuration (SEN0395)
    int uart_tx_pin;
    int uart_rx_pin;
    int uart_num;
    uint32_t uart_baud_rate;

    // Task Configuration
    uint8_t env_task_core;          // Core for environmental sensor task
    uint8_t reactive_task_core;     // Core for gesture/ToF task
    uint8_t env_task_priority;
    uint8_t reactive_task_priority;

    // Thresholds
    uint16_t proximity_threshold_mm;    // VL53L1X proximity threshold
    uint16_t darkness_threshold_lux;    // VEML7700 darkness threshold
    uint16_t brightness_threshold_lux;  // VEML7700 brightness threshold
} sensor_hub_config_t;

/**
 * @brief Default configuration macro
 */
#define SENSOR_HUB_DEFAULT_CONFIG() { \
    .i2c_sda_pin = 7, \
    .i2c_scl_pin = 8, \
    .i2c_freq_hz = 100000, \
    .uart_tx_pin = 17, \
    .uart_rx_pin = 18, \
    .uart_num = 1, \
    .uart_baud_rate = 115200, \
    .env_task_core = 0, \
    .reactive_task_core = 0, \
    .env_task_priority = 5, \
    .reactive_task_priority = 10, \
    .proximity_threshold_mm = 500, \
    .darkness_threshold_lux = 10, \
    .brightness_threshold_lux = 500, \
}

// =============================================================================
// Callback Types
// =============================================================================

/**
 * @brief Gesture event callback
 */
typedef void (*gesture_callback_t)(gesture_type_t gesture, void *user_data);

/**
 * @brief Proximity change callback
 */
typedef void (*proximity_callback_t)(bool is_close, uint16_t distance_mm, void *user_data);

/**
 * @brief Presence change callback
 */
typedef void (*presence_callback_t)(bool is_present, void *user_data);

// =============================================================================
// Public API
// =============================================================================

/**
 * @brief Initialize the sensor hub
 *
 * Initializes all sensors, creates mutex for thread-safe access,
 * and starts the sensor monitoring tasks.
 *
 * @param config Configuration parameters
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sensor_hub_init(const sensor_hub_config_t *config);

/**
 * @brief Deinitialize the sensor hub
 *
 * Stops all tasks, releases resources, and powers down sensors.
 *
 * @return ESP_OK on success
 */
esp_err_t sensor_hub_deinit(void);

/**
 * @brief Get a thread-safe copy of room environment data
 *
 * @param data Pointer to structure to fill with current data
 * @return ESP_OK on success
 */
esp_err_t sensor_hub_get_data(room_environment_t *data);

/**
 * @brief Get air quality data only
 *
 * @param data Pointer to air quality structure
 * @return ESP_OK on success
 */
esp_err_t sensor_hub_get_air_quality(air_quality_data_t *data);

/**
 * @brief Get presence data only
 *
 * @param data Pointer to presence structure
 * @return ESP_OK on success
 */
esp_err_t sensor_hub_get_presence(presence_data_t *data);

/**
 * @brief Get latest gesture
 *
 * @param gesture Pointer to store gesture type
 * @return ESP_OK if gesture available, ESP_ERR_NOT_FOUND if no pending gesture
 */
esp_err_t sensor_hub_get_gesture(gesture_type_t *gesture);

/**
 * @brief Get ambient light data
 *
 * @param data Pointer to light data structure
 * @return ESP_OK on success
 */
esp_err_t sensor_hub_get_light(ambient_light_data_t *data);

/**
 * @brief Register gesture event callback
 *
 * @param callback Function to call on gesture detection
 * @param user_data User data passed to callback
 * @return ESP_OK on success
 */
esp_err_t sensor_hub_register_gesture_callback(gesture_callback_t callback, void *user_data);

/**
 * @brief Register proximity change callback
 *
 * @param callback Function to call on proximity change
 * @param user_data User data passed to callback
 * @return ESP_OK on success
 */
esp_err_t sensor_hub_register_proximity_callback(proximity_callback_t callback, void *user_data);

/**
 * @brief Register presence change callback
 *
 * @param callback Function to call on presence change
 * @param user_data User data passed to callback
 * @return ESP_OK on success
 */
esp_err_t sensor_hub_register_presence_callback(presence_callback_t callback, void *user_data);

/**
 * @brief Get event group handle for external synchronization
 *
 * @return Event group handle
 */
EventGroupHandle_t sensor_hub_get_event_group(void);

/**
 * @brief Check if a specific sensor is online
 *
 * @param sensor_name Name of sensor ("sen66", "sen0395", "vl53l1x", "paj7620", "veml7700")
 * @return true if sensor is online and responding
 */
bool sensor_hub_is_sensor_online(const char *sensor_name);

/**
 * @brief Force immediate sensor reading
 *
 * Useful for on-demand updates before critical operations.
 *
 * @return ESP_OK on success
 */
esp_err_t sensor_hub_force_update(void);

/**
 * @brief Get human-readable gesture name
 *
 * @param gesture Gesture type
 * @return Static string with gesture name
 */
const char *sensor_hub_gesture_to_string(gesture_type_t gesture);

/**
 * @brief Get human-readable AQI level name
 *
 * @param level AQI level
 * @return Static string with AQI level name
 */
const char *sensor_hub_aqi_to_string(aqi_level_t level);

#ifdef __cplusplus
}
#endif
