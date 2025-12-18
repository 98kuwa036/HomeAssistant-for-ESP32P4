/**
 * @file sen0395.h
 * @brief DFRobot SEN0395 mmWave Radar Driver (Human Presence Detection)
 *
 * 24GHz mmWave radar for human presence detection:
 *   - Stationary and moving human detection
 *   - Distance measurement (0-9m)
 *   - Motion and static energy levels
 *
 * Communication: UART @ 115200 baud
 * Protocol: ASCII command/response + binary data frames
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

// UART Configuration
#define SEN0395_DEFAULT_BAUD_RATE   115200
#define SEN0395_UART_BUF_SIZE       512

// Detection ranges
#define SEN0395_MIN_RANGE_CM        0
#define SEN0395_MAX_RANGE_CM        900

// Frame markers
#define SEN0395_FRAME_HEADER        0xAA
#define SEN0395_FRAME_END           0x55

/**
 * @brief Presence detection result
 */
typedef struct {
    bool is_present;            // Human detected
    bool is_moving;             // Motion detected
    uint16_t distance_cm;       // Distance to target
    uint8_t motion_energy;      // Motion energy (0-100)
    uint8_t static_energy;      // Static energy (0-100)
    uint32_t timestamp_ms;      // Measurement timestamp
} sen0395_data_t;

/**
 * @brief Configuration structure
 */
typedef struct {
    uart_port_t uart_num;
    int tx_pin;
    int rx_pin;
    uint32_t baud_rate;
    uint16_t detection_range_max_cm;
    uint8_t sensitivity;
    uint16_t unmanned_delay_s;
} sen0395_config_t;

#define SEN0395_DEFAULT_CONFIG() { \
    .uart_num = UART_NUM_1, \
    .tx_pin = 17, \
    .rx_pin = 18, \
    .baud_rate = 115200, \
    .detection_range_max_cm = 600, \
    .sensitivity = 7, \
    .unmanned_delay_s = 15, \
}

esp_err_t sen0395_init(const sen0395_config_t *config);
esp_err_t sen0395_deinit(void);
esp_err_t sen0395_read_data(sen0395_data_t *data);
esp_err_t sen0395_set_range(uint16_t max_cm);
esp_err_t sen0395_set_sensitivity(uint8_t sensitivity);
esp_err_t sen0395_save_config(void);
bool sen0395_is_connected(void);
esp_err_t sen0395_process(void);

#ifdef __cplusplus
}
#endif
