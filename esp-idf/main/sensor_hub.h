/**
 * @file sensor_hub.h
 * @brief Omni-P4: Multi-Sensor Integration Hub
 *
 * Unified sensor management for ESP32-P4 Smart Home Console:
 *   - SCD41 (I2C): CO2, Temperature, Humidity
 *   - SGP40 (I2C): VOC Index
 *   - ENS160 (I2C): eCO2, TVOC, AQI
 *   - BMP388 (I2C): Barometric pressure, Altitude
 *   - SHT40 (I2C): High-precision Temperature/Humidity
 *   - SEN0428 (I2C): PM1.0, PM2.5, PM10
 *   - SEN0395 (UART): mmWave presence detection
 *   - SEN0540 (UART): Offline voice recognition
 *
 * Architecture:
 *   - Thread-safe data access via mutex
 *   - Single environment task for all I2C sensors
 *   - Separate UART processing for presence/voice
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

#define SENSOR_AIR_QUALITY_BIT      BIT0
#define SENSOR_PRESENCE_EVENT_BIT   BIT1
#define SENSOR_VOICE_CMD_BIT        BIT2
#define SENSOR_PRESSURE_CHANGE_BIT  BIT3
#define SENSOR_DATA_READY_BIT       BIT4

// =============================================================================
// Air Quality Index Levels (ENS160 based)
// =============================================================================

typedef enum {
    AQI_EXCELLENT = 1,
    AQI_GOOD = 2,
    AQI_MODERATE = 3,
    AQI_POOR = 4,
    AQI_UNHEALTHY = 5,
} aqi_level_t;

// =============================================================================
// Data Structures
// =============================================================================

/**
 * @brief CO2 and basic environment data (SCD41)
 */
typedef struct {
    uint16_t co2_ppm;               // CO2 concentration (400-5000 ppm)
    float temperature;              // Temperature (°C)
    float humidity;                 // Relative Humidity (%)
    bool valid;
    uint32_t timestamp_ms;
} co2_data_t;

/**
 * @brief VOC data (SGP40)
 */
typedef struct {
    int32_t voc_index;              // VOC Index (1-500, 100 = normal)
    uint16_t voc_raw;               // Raw sensor value
    uint32_t timestamp_ms;
} voc_data_t;

/**
 * @brief Air quality data (ENS160)
 */
typedef struct {
    uint16_t eco2;                  // Equivalent CO2 (ppm)
    uint16_t tvoc;                  // Total VOC (ppb)
    aqi_level_t aqi;                // Air Quality Index (1-5)
    uint32_t timestamp_ms;
} ens_air_quality_t;

/**
 * @brief Barometric data (BMP388)
 */
typedef struct {
    float pressure;                 // Pressure (hPa)
    float temperature;              // Temperature (°C)
    float altitude;                 // Calculated altitude (m)
    uint32_t timestamp_ms;
} barometric_data_t;

/**
 * @brief High-precision temperature/humidity (SHT40)
 */
typedef struct {
    float temperature;              // Temperature (°C)
    float humidity;                 // Relative Humidity (%)
    uint32_t timestamp_ms;
} precision_env_data_t;

/**
 * @brief Particulate matter data (SEN0428)
 */
typedef struct {
    uint16_t pm1_0;                 // PM1.0 (µg/m³)
    uint16_t pm2_5;                 // PM2.5 (µg/m³)
    uint16_t pm10;                  // PM10 (µg/m³)
    uint32_t timestamp_ms;
} pm_data_t;

/**
 * @brief Presence detection (SEN0395)
 */
typedef struct {
    bool is_present;                // Human detected
    bool is_moving;                 // Motion detected
    uint16_t distance_cm;           // Distance to target
    uint32_t presence_duration_ms;  // Duration of presence
    uint32_t timestamp_ms;
} presence_data_t;

/**
 * @brief Voice command data (SEN0540)
 */
typedef struct {
    uint8_t cmd_id;                 // Command ID
    char keyword[32];               // Recognized keyword
    bool pending;                   // New command pending
    uint32_t timestamp_ms;
} voice_cmd_data_t;

/**
 * @brief Complete room environment data
 */
typedef struct {
    co2_data_t co2;                 // SCD41
    voc_data_t voc;                 // SGP40
    ens_air_quality_t ens_aqi;      // ENS160
    barometric_data_t baro;         // BMP388
    precision_env_data_t precision; // SHT40
    pm_data_t pm;                   // SEN0428
    presence_data_t presence;       // SEN0395
    voice_cmd_data_t voice;         // SEN0540

    // Sensor status
    struct {
        bool scd41_online;
        bool sgp40_online;
        bool ens160_online;
        bool bmp388_online;
        bool sht40_online;
        bool sen0428_online;
        bool sen0395_online;
        bool sen0540_online;
    } status;

    uint32_t system_uptime_ms;
} room_environment_t;

// =============================================================================
// Configuration
// =============================================================================

typedef struct {
    // I2C Configuration
    int i2c_sda_pin;
    int i2c_scl_pin;
    uint32_t i2c_freq_hz;

    // UART1 Configuration (SEN0395 mmWave)
    int uart1_tx_pin;
    int uart1_rx_pin;

    // UART2 Configuration (SEN0540 Voice)
    int uart2_tx_pin;
    int uart2_rx_pin;

    // Task Configuration
    uint8_t task_core;
    uint8_t task_priority;

    // Altitude for pressure compensation
    uint16_t altitude_m;
} sensor_hub_config_t;

#define SENSOR_HUB_DEFAULT_CONFIG() { \
    .i2c_sda_pin = 7, \
    .i2c_scl_pin = 8, \
    .i2c_freq_hz = 100000, \
    .uart1_tx_pin = 17, \
    .uart1_rx_pin = 18, \
    .uart2_tx_pin = 4, \
    .uart2_rx_pin = 5, \
    .task_core = 0, \
    .task_priority = 5, \
    .altitude_m = 0, \
}

// =============================================================================
// Callbacks
// =============================================================================

typedef void (*presence_callback_t)(bool is_present, void *user_data);
typedef void (*voice_cmd_callback_t)(uint8_t cmd_id, const char *keyword, void *user_data);

// =============================================================================
// Public API
// =============================================================================

esp_err_t sensor_hub_init(const sensor_hub_config_t *config);
esp_err_t sensor_hub_deinit(void);

// Data retrieval
esp_err_t sensor_hub_get_data(room_environment_t *data);
esp_err_t sensor_hub_get_co2(co2_data_t *data);
esp_err_t sensor_hub_get_voc(voc_data_t *data);
esp_err_t sensor_hub_get_aqi(ens_air_quality_t *data);
esp_err_t sensor_hub_get_barometric(barometric_data_t *data);
esp_err_t sensor_hub_get_precision_env(precision_env_data_t *data);
esp_err_t sensor_hub_get_pm(pm_data_t *data);
esp_err_t sensor_hub_get_presence(presence_data_t *data);
esp_err_t sensor_hub_get_voice_cmd(voice_cmd_data_t *data);

// Callbacks
esp_err_t sensor_hub_register_presence_callback(presence_callback_t callback, void *user_data);
esp_err_t sensor_hub_register_voice_callback(voice_cmd_callback_t callback, void *user_data);

// Utilities
EventGroupHandle_t sensor_hub_get_event_group(void);
bool sensor_hub_is_sensor_online(const char *sensor_name);
const char *sensor_hub_aqi_to_string(aqi_level_t level);

#ifdef __cplusplus
}
#endif
