/**
 * @file veml7700.h
 * @brief VEML7700 Ambient Light Sensor Driver
 *
 * Vishay VEML7700 features:
 *   - High accuracy ambient light sensing
 *   - 16-bit resolution
 *   - Range: 0 to 120,000 lux
 *   - White channel + ALS channel
 *   - Programmable gain and integration time
 *
 * I2C Address: 0x10
 * I2C Speed: Up to 400kHz
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

// I2C Address
#define VEML7700_I2C_ADDR           0x10

// Gain settings
typedef enum {
    VEML7700_GAIN_1X = 0,       // x1
    VEML7700_GAIN_2X = 1,       // x2
    VEML7700_GAIN_1_8X = 2,     // x1/8
    VEML7700_GAIN_1_4X = 3,     // x1/4
} veml7700_gain_t;

// Integration time settings
typedef enum {
    VEML7700_IT_25MS = 12,      // 25ms
    VEML7700_IT_50MS = 8,       // 50ms
    VEML7700_IT_100MS = 0,      // 100ms (default)
    VEML7700_IT_200MS = 1,      // 200ms
    VEML7700_IT_400MS = 2,      // 400ms
    VEML7700_IT_800MS = 3,      // 800ms
} veml7700_integration_time_t;

/**
 * @brief Light measurement data
 */
typedef struct {
    uint32_t lux;               // Calculated lux value
    uint16_t als_raw;           // Raw ALS channel value
    uint16_t white_raw;         // Raw white channel value
    uint32_t timestamp_ms;      // Measurement timestamp
} veml7700_data_t;

/**
 * @brief Configuration
 */
typedef struct {
    i2c_port_t i2c_port;
    veml7700_gain_t gain;
    veml7700_integration_time_t integration_time;
    bool auto_gain;             // Auto-adjust gain for optimal range
} veml7700_config_t;

#define VEML7700_DEFAULT_CONFIG() { \
    .i2c_port = I2C_NUM_0, \
    .gain = VEML7700_GAIN_1X, \
    .integration_time = VEML7700_IT_100MS, \
    .auto_gain = true, \
}

esp_err_t veml7700_init(const veml7700_config_t *config);
esp_err_t veml7700_deinit(void);
esp_err_t veml7700_read_data(veml7700_data_t *data);
esp_err_t veml7700_read_lux(uint32_t *lux);
esp_err_t veml7700_set_gain(veml7700_gain_t gain);
esp_err_t veml7700_set_integration_time(veml7700_integration_time_t it);
esp_err_t veml7700_power_on(void);
esp_err_t veml7700_power_off(void);
bool veml7700_is_connected(void);

#ifdef __cplusplus
}
#endif
