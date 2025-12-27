/**
 * @file sgp40.h
 * @brief Sensirion SGP40 VOC Sensor Driver
 *
 * SGP40 features:
 *   - VOC Index output (1-500, 100 = normal)
 *   - Built-in humidity compensation
 *   - Fast response time
 *   - Long-term stability
 *
 * I2C Address: 0x59
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SGP40_I2C_ADDR              0x59

/**
 * @brief SGP40 measurement data
 */
typedef struct {
    uint16_t voc_raw;               // Raw VOC signal
    int32_t voc_index;              // VOC Index (1-500, 100 = normal)
    uint32_t timestamp_ms;
} sgp40_data_t;

typedef struct {
    i2c_port_t i2c_port;
} sgp40_config_t;

#define SGP40_DEFAULT_CONFIG() { \
    .i2c_port = I2C_NUM_0, \
}

esp_err_t sgp40_init(const sgp40_config_t *config);
esp_err_t sgp40_deinit(void);
esp_err_t sgp40_measure_raw(uint16_t *raw_signal, float humidity, float temperature);
esp_err_t sgp40_read_voc_index(sgp40_data_t *data, float humidity, float temperature);
esp_err_t sgp40_heater_off(void);
bool sgp40_is_connected(void);

#ifdef __cplusplus
}
#endif
