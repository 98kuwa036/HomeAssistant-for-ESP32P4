/**
 * @file sen66.h
 * @brief SEN66 Environmental Sensor Node Driver
 *
 * Sensirion SEN66 is an all-in-one sensor module providing:
 *   - CO2 concentration (photoacoustic)
 *   - PM1.0, PM2.5, PM4.0, PM10 (laser scattering)
 *   - VOC Index (MOx sensor)
 *   - NOx Index (MOx sensor)
 *   - Temperature and Humidity
 *
 * I2C Address: 0x6B
 * I2C Speed: 100kHz (max)
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
#define SEN66_I2C_ADDR              0x6B

// Command codes (16-bit)
#define SEN66_CMD_START_MEASUREMENT     0x0021
#define SEN66_CMD_STOP_MEASUREMENT      0x0104
#define SEN66_CMD_READ_DATA_READY       0x0202
#define SEN66_CMD_READ_MEASURED_VALUES  0x03C4
#define SEN66_CMD_READ_PRODUCT_NAME     0xD014
#define SEN66_CMD_READ_SERIAL_NUMBER    0xD033
#define SEN66_CMD_READ_FIRMWARE_VERSION 0xD100
#define SEN66_CMD_DEVICE_RESET          0xD304

// Timing constants
#define SEN66_STARTUP_TIME_MS           1000
#define SEN66_MEASUREMENT_INTERVAL_MS   1000
#define SEN66_COMMAND_DELAY_MS          20

/**
 * @brief SEN66 measurement data structure
 */
typedef struct {
    // Particulate Matter (µg/m³)
    float pm1_0;
    float pm2_5;
    float pm4_0;
    float pm10;

    // Gas indices
    float voc_index;    // 1-500, 100 = normal
    float nox_index;    // 1-500, 1 = normal

    // CO2 (ppm)
    float co2;

    // Environmental
    float temperature;  // °C
    float humidity;     // %RH

    // Status
    bool data_valid;
} sen66_data_t;

/**
 * @brief SEN66 device information
 */
typedef struct {
    char product_name[32];
    char serial_number[32];
    uint8_t firmware_major;
    uint8_t firmware_minor;
} sen66_device_info_t;

/**
 * @brief Initialize SEN66 sensor
 *
 * @param i2c_port I2C port number
 * @return ESP_OK on success
 */
esp_err_t sen66_init(i2c_port_t i2c_port);

/**
 * @brief Deinitialize SEN66 sensor
 *
 * @return ESP_OK on success
 */
esp_err_t sen66_deinit(void);

/**
 * @brief Start continuous measurement
 *
 * @return ESP_OK on success
 */
esp_err_t sen66_start_measurement(void);

/**
 * @brief Stop measurement
 *
 * @return ESP_OK on success
 */
esp_err_t sen66_stop_measurement(void);

/**
 * @brief Check if new data is available
 *
 * @param ready Pointer to store readiness status
 * @return ESP_OK on success
 */
esp_err_t sen66_is_data_ready(bool *ready);

/**
 * @brief Read measurement data
 *
 * @param data Pointer to data structure to fill
 * @return ESP_OK on success
 */
esp_err_t sen66_read_data(sen66_data_t *data);

/**
 * @brief Read device information
 *
 * @param info Pointer to info structure to fill
 * @return ESP_OK on success
 */
esp_err_t sen66_get_device_info(sen66_device_info_t *info);

/**
 * @brief Reset device
 *
 * @return ESP_OK on success
 */
esp_err_t sen66_reset(void);

/**
 * @brief Check if sensor is connected and responding
 *
 * @return true if sensor responds
 */
bool sen66_is_connected(void);

#ifdef __cplusplus
}
#endif
