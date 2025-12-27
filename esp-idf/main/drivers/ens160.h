/**
 * @file ens160.h
 * @brief ScioSense ENS160 Digital Metal-Oxide Air Quality Sensor Driver
 *
 * ENS160 features:
 *   - eCO2 output: 400-65000 ppm
 *   - TVOC output: 0-65000 ppb
 *   - Air Quality Index (AQI): 1-5
 *   - Independent heater control
 *   - Temperature/humidity compensation
 *
 * I2C Address: 0x52 (default) or 0x53
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ENS160_I2C_ADDR_DEFAULT     0x52
#define ENS160_I2C_ADDR_ALT         0x53

// Operating modes
typedef enum {
    ENS160_MODE_DEEP_SLEEP = 0,
    ENS160_MODE_IDLE = 1,
    ENS160_MODE_STANDARD = 2,
} ens160_mode_t;

// Air Quality Index levels
typedef enum {
    ENS160_AQI_EXCELLENT = 1,
    ENS160_AQI_GOOD = 2,
    ENS160_AQI_MODERATE = 3,
    ENS160_AQI_POOR = 4,
    ENS160_AQI_UNHEALTHY = 5,
} ens160_aqi_t;

/**
 * @brief ENS160 measurement data
 */
typedef struct {
    uint16_t eco2;                  // eCO2 (ppm)
    uint16_t tvoc;                  // TVOC (ppb)
    ens160_aqi_t aqi;               // Air Quality Index (1-5)
    uint8_t status;                 // Status register
    bool data_valid;                // New data available
    uint32_t timestamp_ms;
} ens160_data_t;

typedef struct {
    i2c_port_t i2c_port;
    uint8_t i2c_addr;
} ens160_config_t;

#define ENS160_DEFAULT_CONFIG() { \
    .i2c_port = I2C_NUM_0, \
    .i2c_addr = ENS160_I2C_ADDR_DEFAULT, \
}

esp_err_t ens160_init(const ens160_config_t *config);
esp_err_t ens160_deinit(void);
esp_err_t ens160_set_mode(ens160_mode_t mode);
esp_err_t ens160_read_data(ens160_data_t *data);
esp_err_t ens160_set_compensation(float temperature, float humidity);
bool ens160_is_connected(void);
const char *ens160_aqi_to_string(ens160_aqi_t aqi);

#ifdef __cplusplus
}
#endif
