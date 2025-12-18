/**
 * @file vl53l1x.h
 * @brief VL53L1X Time-of-Flight Distance Sensor Driver
 *
 * STMicroelectronics VL53L1X features:
 *   - Range: 40mm to 4000mm
 *   - Accuracy: ±3% typical
 *   - Field of View: 27°
 *   - Fast ranging: up to 50Hz
 *
 * I2C Address: 0x29 (default)
 * I2C Speed: Up to 400kHz (Fast Mode)
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
#define VL53L1X_I2C_ADDR            0x29

// Distance modes
typedef enum {
    VL53L1X_DISTANCE_MODE_SHORT = 1,    // Up to 1.3m, better ambient immunity
    VL53L1X_DISTANCE_MODE_LONG = 2,     // Up to 4m, maximum range
} vl53l1x_distance_mode_t;

// Timing budget (ms) - affects accuracy and power
typedef enum {
    VL53L1X_TIMING_15MS = 15,
    VL53L1X_TIMING_20MS = 20,
    VL53L1X_TIMING_33MS = 33,
    VL53L1X_TIMING_50MS = 50,
    VL53L1X_TIMING_100MS = 100,
    VL53L1X_TIMING_200MS = 200,
    VL53L1X_TIMING_500MS = 500,
} vl53l1x_timing_budget_t;

// Range status
typedef enum {
    VL53L1X_RANGE_VALID = 0,
    VL53L1X_RANGE_SIGMA_FAIL = 1,
    VL53L1X_RANGE_SIGNAL_FAIL = 2,
    VL53L1X_RANGE_OUT_OF_BOUNDS = 4,
    VL53L1X_RANGE_WRAP_AROUND = 7,
} vl53l1x_range_status_t;

/**
 * @brief Measurement result
 */
typedef struct {
    uint16_t distance_mm;           // Distance in millimeters
    uint16_t signal_rate;           // Signal rate (MCPS)
    uint16_t ambient_rate;          // Ambient light rate
    uint8_t range_status;           // Validity status
    bool valid;                     // Quick validity check
    uint32_t timestamp_ms;          // Measurement timestamp
} vl53l1x_data_t;

/**
 * @brief Configuration
 */
typedef struct {
    i2c_port_t i2c_port;
    vl53l1x_distance_mode_t distance_mode;
    vl53l1x_timing_budget_t timing_budget_ms;
    uint16_t inter_measurement_ms;      // 0 = single shot mode
} vl53l1x_config_t;

#define VL53L1X_DEFAULT_CONFIG() { \
    .i2c_port = I2C_NUM_0, \
    .distance_mode = VL53L1X_DISTANCE_MODE_LONG, \
    .timing_budget_ms = VL53L1X_TIMING_50MS, \
    .inter_measurement_ms = 50, \
}

esp_err_t vl53l1x_init(const vl53l1x_config_t *config);
esp_err_t vl53l1x_deinit(void);
esp_err_t vl53l1x_start_ranging(void);
esp_err_t vl53l1x_stop_ranging(void);
esp_err_t vl53l1x_read_data(vl53l1x_data_t *data);
esp_err_t vl53l1x_set_distance_mode(vl53l1x_distance_mode_t mode);
esp_err_t vl53l1x_set_timing_budget(vl53l1x_timing_budget_t budget_ms);
esp_err_t vl53l1x_set_inter_measurement(uint16_t period_ms);
bool vl53l1x_is_data_ready(void);
bool vl53l1x_is_connected(void);

#ifdef __cplusplus
}
#endif
