/**
 * @file sensor_hub.h
 * @brief Multi-Sensor Hub for Omni-P4
 *
 * Manages 8 environmental sensors:
 *   I2C Bus:
 *     - SHT40: Temperature & Humidity (Sensirion)
 *     - BMP388: Barometric Pressure (Bosch)
 *     - SCD41: CO2 Concentration (Sensirion)
 *     - ENS160: TVOC & eCO2 (ScioSense)
 *     - SGP40: VOC Index (Sensirion)
 *     - BH1750: Ambient Light (Rohm)
 *
 *   UART:
 *     - LD2410: mmWave Presence Radar (Hi-Link)
 *     - SEN0540: Offline Voice Recognition (DFRobot)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Data Structures
// ============================================================================

/**
 * @brief Air quality level enumeration
 */
typedef enum {
    AIR_QUALITY_EXCELLENT = 1,  // AQI 1: Excellent
    AIR_QUALITY_GOOD = 2,       // AQI 2: Good
    AIR_QUALITY_MODERATE = 3,   // AQI 3: Moderate
    AIR_QUALITY_POOR = 4,       // AQI 4: Poor
    AIR_QUALITY_UNHEALTHY = 5   // AQI 5: Unhealthy
} air_quality_level_t;

/**
 * @brief Presence detection state
 */
typedef enum {
    PRESENCE_NONE = 0,       // No one detected
    PRESENCE_STATIONARY,     // Person present, stationary
    PRESENCE_MOVING          // Person present, moving
} presence_state_t;

/**
 * @brief Aggregated sensor data structure
 */
typedef struct {
    // Timestamp
    int64_t timestamp_us;        // Microseconds since boot

    // SHT40 - Temperature & Humidity
    float temperature;           // °C
    float humidity;              // %RH
    bool sht40_valid;

    // BMP388 - Barometric Pressure
    float pressure;              // hPa
    float altitude;              // meters (calculated)
    bool bmp388_valid;

    // SCD41 - CO2
    uint16_t co2;                // ppm
    float scd41_temperature;     // °C (backup)
    float scd41_humidity;        // %RH (backup)
    bool scd41_valid;

    // ENS160 - Air Quality
    uint16_t tvoc;               // ppb
    uint16_t eco2;               // ppm (calculated)
    air_quality_level_t aqi;     // 1-5
    bool ens160_valid;

    // SGP40 - VOC Index
    int32_t voc_index;           // 0-500 (100 = typical)
    int32_t voc_raw;             // Raw signal
    bool sgp40_valid;

    // BH1750 - Ambient Light
    float lux;                   // lux
    bool bh1750_valid;

    // LD2410 - Presence Radar
    presence_state_t presence;   // Presence state
    uint16_t presence_distance;  // cm
    uint8_t presence_energy;     // 0-100%
    bool ld2410_valid;

    // SEN0540 - Voice Recognition
    uint8_t voice_command_id;    // Last recognized command (0 = none)
    bool voice_command_new;      // New command received flag
    bool sen0540_valid;

} sensor_data_t;

/**
 * @brief Sensor status for diagnostics
 */
typedef struct {
    bool sht40_present;
    bool bmp388_present;
    bool scd41_present;
    bool ens160_present;
    bool sgp40_present;
    bool bh1750_present;
    bool ld2410_present;
    bool sen0540_present;

    uint32_t i2c_errors;
    uint32_t uart_errors;
    uint32_t read_count;
} sensor_status_t;

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Initialize sensor hub
 *
 * Initializes I2C bus, UART ports, and all sensors.
 *
 * @return ESP_OK on success
 */
esp_err_t sensor_hub_init(void);

/**
 * @brief Deinitialize sensor hub
 */
void sensor_hub_deinit(void);

/**
 * @brief Read all sensors
 *
 * Reads all connected sensors and populates the data structure.
 * Invalid sensors will have their valid flags set to false.
 *
 * @param data Pointer to sensor data structure
 * @return ESP_OK on success (even if some sensors fail)
 */
esp_err_t sensor_hub_read_all(sensor_data_t *data);

/**
 * @brief Read specific sensor group
 */
esp_err_t sensor_hub_read_climate(sensor_data_t *data);
esp_err_t sensor_hub_read_air_quality(sensor_data_t *data);
esp_err_t sensor_hub_read_presence(sensor_data_t *data);
esp_err_t sensor_hub_read_light(sensor_data_t *data);

/**
 * @brief Get sensor status for diagnostics
 */
void sensor_hub_get_status(sensor_status_t *status);

/**
 * @brief Force sensor calibration (for SCD41)
 * @param reference_co2 Known CO2 value in ppm
 */
esp_err_t sensor_hub_calibrate_co2(uint16_t reference_co2);

/**
 * @brief Convert sensor data to JSON string
 *
 * @param data Sensor data structure
 * @param json_buf Output buffer for JSON string
 * @param buf_len Buffer length
 * @return Number of bytes written, or -1 on error
 */
int sensor_hub_to_json(const sensor_data_t *data, char *json_buf, size_t buf_len);

/**
 * @brief Get human-readable air quality description
 */
const char *sensor_hub_aqi_description(air_quality_level_t aqi);

/**
 * @brief Check if sensor hub is initialized
 */
bool sensor_hub_is_initialized(void);

#ifdef __cplusplus
}
#endif
