/**
 * @file bmp388.h
 * @brief Bosch BMP388 Barometric Pressure Sensor Driver
 *
 * BMP388 features:
 *   - Pressure: 300-1250 hPa (±0.5 hPa accuracy)
 *   - Temperature: -40 to +85°C (±0.5°C accuracy)
 *   - Altitude calculation support
 *   - Low power consumption
 *
 * I2C Address: 0x76 (default) or 0x77
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BMP388_I2C_ADDR_DEFAULT     0x77
#define BMP388_I2C_ADDR_ALT         0x76

// Oversampling rates
typedef enum {
    BMP388_OSR_X1 = 0,
    BMP388_OSR_X2 = 1,
    BMP388_OSR_X4 = 2,
    BMP388_OSR_X8 = 3,
    BMP388_OSR_X16 = 4,
    BMP388_OSR_X32 = 5,
} bmp388_osr_t;

// Output data rate
typedef enum {
    BMP388_ODR_200HZ = 0x00,
    BMP388_ODR_100HZ = 0x01,
    BMP388_ODR_50HZ = 0x02,
    BMP388_ODR_25HZ = 0x03,
    BMP388_ODR_12_5HZ = 0x04,
    BMP388_ODR_6_25HZ = 0x05,
    BMP388_ODR_1HZ = 0x11,
} bmp388_odr_t;

/**
 * @brief BMP388 measurement data
 */
typedef struct {
    float pressure;                 // Pressure (hPa)
    float temperature;              // Temperature (°C)
    float altitude;                 // Calculated altitude (m)
    uint32_t timestamp_ms;
} bmp388_data_t;

typedef struct {
    i2c_port_t i2c_port;
    uint8_t i2c_addr;
    bmp388_osr_t pressure_osr;
    bmp388_osr_t temperature_osr;
    bmp388_odr_t odr;
    float sea_level_pressure;       // For altitude calculation (hPa)
} bmp388_config_t;

#define BMP388_DEFAULT_CONFIG() { \
    .i2c_port = I2C_NUM_0, \
    .i2c_addr = BMP388_I2C_ADDR_DEFAULT, \
    .pressure_osr = BMP388_OSR_X8, \
    .temperature_osr = BMP388_OSR_X1, \
    .odr = BMP388_ODR_50HZ, \
    .sea_level_pressure = 1013.25f, \
}

esp_err_t bmp388_init(const bmp388_config_t *config);
esp_err_t bmp388_deinit(void);
esp_err_t bmp388_read_data(bmp388_data_t *data);
esp_err_t bmp388_set_sea_level_pressure(float pressure);
bool bmp388_is_connected(void);

#ifdef __cplusplus
}
#endif
