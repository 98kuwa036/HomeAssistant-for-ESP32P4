/**
 * @file sht40.h
 * @brief Sensirion SHT40 Temperature/Humidity Sensor Driver
 *
 * SHT40 features:
 *   - Temperature: -40 to +125°C (±0.2°C accuracy)
 *   - Humidity: 0-100% RH (±1.8% accuracy)
 *   - Fast response time
 *   - Built-in heater for condensation recovery
 *
 * I2C Address: 0x44 (default), 0x45, 0x46
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHT40_I2C_ADDR              0x44

// Precision modes
typedef enum {
    SHT40_PRECISION_HIGH,           // High precision (8.2ms)
    SHT40_PRECISION_MEDIUM,         // Medium precision (4.5ms)
    SHT40_PRECISION_LOW,            // Low precision (1.7ms)
} sht40_precision_t;

/**
 * @brief SHT40 measurement data
 */
typedef struct {
    float temperature;              // Temperature (°C)
    float humidity;                 // Relative humidity (%)
    uint32_t timestamp_ms;
} sht40_data_t;

typedef struct {
    i2c_port_t i2c_port;
    sht40_precision_t precision;
} sht40_config_t;

#define SHT40_DEFAULT_CONFIG() { \
    .i2c_port = I2C_NUM_0, \
    .precision = SHT40_PRECISION_HIGH, \
}

esp_err_t sht40_init(const sht40_config_t *config);
esp_err_t sht40_deinit(void);
esp_err_t sht40_read_data(sht40_data_t *data);
esp_err_t sht40_activate_heater(uint16_t duration_ms);
esp_err_t sht40_soft_reset(void);
esp_err_t sht40_get_serial(uint32_t *serial);
bool sht40_is_connected(void);

#ifdef __cplusplus
}
#endif
