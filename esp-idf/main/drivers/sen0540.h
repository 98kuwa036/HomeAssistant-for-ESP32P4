/**
 * @file sen0540.h
 * @brief DFRobot SEN0540 Offline Voice Recognition Module Driver
 *
 * SEN0540 features:
 *   - Offline voice recognition (no cloud required)
 *   - Up to 121 custom keywords
 *   - Multiple wake words support
 *   - Chinese and English support
 *   - Low power consumption
 *
 * Communication: UART @ 9600 baud
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SEN0540_BAUD_RATE           9600
#define SEN0540_MAX_KEYWORDS        121

// Recognition result callback
typedef void (*sen0540_callback_t)(uint8_t cmd_id, const char *keyword, void *user_data);

/**
 * @brief Voice recognition result
 */
typedef struct {
    uint8_t cmd_id;                 // Command ID (0-120)
    char keyword[32];               // Recognized keyword
    uint8_t confidence;             // Recognition confidence (0-100)
    uint32_t timestamp_ms;
} sen0540_result_t;

/**
 * @brief Configuration structure
 */
typedef struct {
    uart_port_t uart_num;
    int tx_pin;
    int rx_pin;
    uint8_t wake_up_time_s;         // Wake word timeout (0 = always on)
    sen0540_callback_t callback;
    void *callback_data;
} sen0540_config_t;

#define SEN0540_DEFAULT_CONFIG() { \
    .uart_num = UART_NUM_2, \
    .tx_pin = 4, \
    .rx_pin = 5, \
    .wake_up_time_s = 0, \
    .callback = NULL, \
    .callback_data = NULL, \
}

// Built-in command IDs
#define SEN0540_CMD_WAKE_WORD       0
#define SEN0540_CMD_LIGHT_ON        1
#define SEN0540_CMD_LIGHT_OFF       2
#define SEN0540_CMD_VOLUME_UP       3
#define SEN0540_CMD_VOLUME_DOWN     4
#define SEN0540_CMD_PLAY            5
#define SEN0540_CMD_PAUSE           6
#define SEN0540_CMD_NEXT            7
#define SEN0540_CMD_PREVIOUS        8

esp_err_t sen0540_init(const sen0540_config_t *config);
esp_err_t sen0540_deinit(void);
esp_err_t sen0540_get_result(sen0540_result_t *result);
esp_err_t sen0540_add_keyword(uint8_t cmd_id, const char *keyword);
esp_err_t sen0540_delete_keyword(uint8_t cmd_id);
esp_err_t sen0540_set_wake_time(uint8_t seconds);
esp_err_t sen0540_set_mute(bool mute);
esp_err_t sen0540_process(void);
bool sen0540_is_connected(void);

#ifdef __cplusplus
}
#endif
