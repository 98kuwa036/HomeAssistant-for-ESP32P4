/**
 * @file scd41.h
 * @brief Sensirion SCD41 CO2/Temperature/Humidity Sensor Driver
 *
 * SCD41 features:
 *   - CO2 measurement: 400-5000 ppm
 *   - Temperature: -10 to +60°C (±0.8°C accuracy)
 *   - Humidity: 0-100% RH (±6% accuracy)
 *   - Photoacoustic sensing principle
 *   - Low power periodic measurement mode
 *
 * I2C Address: 0x62
 * I2C Speed: Up to 100kHz
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SCD41_I2C_ADDR              0x62

// Measurement modes
typedef enum {
    SCD41_MODE_PERIODIC,            // Standard periodic measurement (5s)
    SCD41_MODE_LOW_POWER,           // Low power periodic (30s)
    SCD41_MODE_SINGLE_SHOT,         // Single shot on demand
} scd41_mode_t;

/**
 * @brief SCD41 measurement data
 */
typedef struct {
    uint16_t co2_ppm;               // CO2 concentration (ppm)
    float temperature;              // Temperature (°C)
    float humidity;                 // Relative humidity (%)
    bool data_valid;                // Data validity flag
    uint32_t timestamp_ms;          // Measurement timestamp
} scd41_data_t;

/**
 * @brief Configuration structure
 */
typedef struct {
    i2c_port_t i2c_port;
    scd41_mode_t mode;
    uint16_t altitude_m;            // Altitude compensation (meters)
    bool auto_calibration;          // Automatic self-calibration (ASC)
} scd41_config_t;

#define SCD41_DEFAULT_CONFIG() { \
    .i2c_port = I2C_NUM_0, \
    .mode = SCD41_MODE_PERIODIC, \
    .altitude_m = 0, \
    .auto_calibration = true, \
}

esp_err_t scd41_init(const scd41_config_t *config);
esp_err_t scd41_deinit(void);
esp_err_t scd41_start_measurement(void);
esp_err_t scd41_stop_measurement(void);
esp_err_t scd41_read_data(scd41_data_t *data);
bool scd41_is_data_ready(void);
esp_err_t scd41_set_altitude(uint16_t altitude_m);
esp_err_t scd41_set_temperature_offset(float offset);
esp_err_t scd41_force_recalibration(uint16_t target_co2_ppm);
esp_err_t scd41_get_serial_number(uint64_t *serial);
bool scd41_is_connected(void);

#ifdef __cplusplus
}
#endif
