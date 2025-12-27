/**
 * @file sen0428.h
 * @brief DFRobot SEN0428 PM2.5 Laser Dust Sensor Driver
 *
 * SEN0428 features:
 *   - PM1.0, PM2.5, PM10 measurement
 *   - Particle count per 0.1L air
 *   - Laser scattering principle
 *   - Standard and atmospheric concentration
 *
 * I2C Address: 0x19
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SEN0428_I2C_ADDR            0x19

/**
 * @brief PM sensor measurement data
 */
typedef struct {
    // Standard particle concentration (µg/m³)
    uint16_t pm1_0_std;
    uint16_t pm2_5_std;
    uint16_t pm10_std;

    // Atmospheric environment concentration (µg/m³)
    uint16_t pm1_0_atm;
    uint16_t pm2_5_atm;
    uint16_t pm10_atm;

    // Particle count per 0.1L air
    uint16_t particles_0_3um;
    uint16_t particles_0_5um;
    uint16_t particles_1_0um;
    uint16_t particles_2_5um;
    uint16_t particles_5_0um;
    uint16_t particles_10um;

    uint32_t timestamp_ms;
} sen0428_data_t;

typedef struct {
    i2c_port_t i2c_port;
} sen0428_config_t;

#define SEN0428_DEFAULT_CONFIG() { \
    .i2c_port = I2C_NUM_0, \
}

esp_err_t sen0428_init(const sen0428_config_t *config);
esp_err_t sen0428_deinit(void);
esp_err_t sen0428_read_data(sen0428_data_t *data);
esp_err_t sen0428_sleep(void);
esp_err_t sen0428_wakeup(void);
bool sen0428_is_connected(void);

#ifdef __cplusplus
}
#endif
