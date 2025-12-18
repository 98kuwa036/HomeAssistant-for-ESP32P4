/**
 * @file sen0395.c
 * @brief DFRobot SEN0395 mmWave Radar Driver Implementation
 */

#include "sen0395.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SEN0395";

static uart_port_t s_uart_num = UART_NUM_1;
static bool s_initialized = false;
static sen0395_data_t s_last_data = {0};
static uint8_t s_rx_buffer[SEN0395_UART_BUF_SIZE];

// Command strings for SEN0395
static const char *CMD_QUERY_PRESENCE = "sensorStart";
static const char *CMD_STOP = "sensorStop";
static const char *CMD_SAVE = "saveCfg 0x45670123 0xCDEF89AB 0x956128C6 0xDF54AC89";
static const char *CMD_RESET = "resetCfg 0x45670123 0xCDEF89AB 0x956128C6 0xDF54AC89";

/**
 * @brief Send command to sensor
 */
static esp_err_t sen0395_send_cmd(const char *cmd)
{
    char cmd_buf[128];
    snprintf(cmd_buf, sizeof(cmd_buf), "%s\r\n", cmd);

    int written = uart_write_bytes(s_uart_num, cmd_buf, strlen(cmd_buf));
    if (written < 0) {
        ESP_LOGE(TAG, "Failed to send command: %s", cmd);
        return ESP_FAIL;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}

/**
 * @brief Parse presence data frame
 * Frame format: $JYBSS,<presence>,<motion>,<distance>,<motion_e>,<static_e>*XX
 */
static esp_err_t sen0395_parse_frame(const char *frame, sen0395_data_t *data)
{
    if (strncmp(frame, "$JYBSS", 6) != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int presence, motion, distance, motion_e, static_e;
    int parsed = sscanf(frame, "$JYBSS,%d,%d,%d,%d,%d",
                        &presence, &motion, &distance, &motion_e, &static_e);

    if (parsed >= 3) {
        data->is_present = (presence == 1);
        data->is_moving = (motion == 1);
        data->distance_cm = (uint16_t)distance;
        data->motion_energy = (parsed >= 4) ? (uint8_t)motion_e : 0;
        data->static_energy = (parsed >= 5) ? (uint8_t)static_e : 0;
        data->timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        return ESP_OK;
    }

    // Alternative simple format: "ON" / "OFF"
    if (strstr(frame, "ON") != NULL) {
        data->is_present = true;
        data->timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        return ESP_OK;
    } else if (strstr(frame, "OFF") != NULL) {
        data->is_present = false;
        data->timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        return ESP_OK;
    }

    return ESP_ERR_INVALID_ARG;
}

esp_err_t sen0395_init(const sen0395_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    s_uart_num = config->uart_num;

    // Configure UART
    uart_config_t uart_config = {
        .baud_rate = config->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_param_config(s_uart_num, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART param config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_set_pin(s_uart_num, config->tx_pin, config->rx_pin, -1, -1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART set pin failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_driver_install(s_uart_num, SEN0395_UART_BUF_SIZE * 2, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Start sensor
    vTaskDelay(pdMS_TO_TICKS(500));  // Wait for sensor boot
    sen0395_send_cmd(CMD_QUERY_PRESENCE);

    s_initialized = true;
    ESP_LOGI(TAG, "SEN0395 initialized on UART%d (TX:%d, RX:%d)",
             s_uart_num, config->tx_pin, config->rx_pin);

    return ESP_OK;
}

esp_err_t sen0395_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    sen0395_send_cmd(CMD_STOP);
    uart_driver_delete(s_uart_num);
    s_initialized = false;

    ESP_LOGI(TAG, "SEN0395 deinitialized");
    return ESP_OK;
}

esp_err_t sen0395_process(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Read available data
    int len = uart_read_bytes(s_uart_num, s_rx_buffer,
                              SEN0395_UART_BUF_SIZE - 1,
                              pdMS_TO_TICKS(20));

    if (len <= 0) {
        return ESP_ERR_NOT_FOUND;
    }

    s_rx_buffer[len] = '\0';

    // Find and parse data frames
    char *line = strtok((char *)s_rx_buffer, "\r\n");
    while (line != NULL) {
        sen0395_data_t temp_data;
        if (sen0395_parse_frame(line, &temp_data) == ESP_OK) {
            memcpy(&s_last_data, &temp_data, sizeof(sen0395_data_t));
        }
        line = strtok(NULL, "\r\n");
    }

    return ESP_OK;
}

esp_err_t sen0395_read_data(sen0395_data_t *data)
{
    if (!s_initialized || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    // Process any pending UART data
    sen0395_process();

    // Return last known data
    memcpy(data, &s_last_data, sizeof(sen0395_data_t));
    return ESP_OK;
}

esp_err_t sen0395_set_range(uint16_t max_cm)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    char cmd[64];
    // Convert cm to sensor units (0.15m resolution)
    uint8_t range_val = (max_cm / 15);
    if (range_val > 60) range_val = 60;  // Max 9m

    snprintf(cmd, sizeof(cmd), "setRange 0 %d", range_val);
    return sen0395_send_cmd(cmd);
}

esp_err_t sen0395_set_sensitivity(uint8_t sensitivity)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (sensitivity > 9) {
        sensitivity = 9;
    }

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "setSensitivity %d %d", sensitivity, sensitivity);
    return sen0395_send_cmd(cmd);
}

esp_err_t sen0395_save_config(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    return sen0395_send_cmd(CMD_SAVE);
}

bool sen0395_is_connected(void)
{
    if (!s_initialized) {
        return false;
    }

    // Clear buffer
    uart_flush(s_uart_num);

    // Send query and wait for response
    sen0395_send_cmd("getVersion");

    vTaskDelay(pdMS_TO_TICKS(200));

    int len = uart_read_bytes(s_uart_num, s_rx_buffer,
                              SEN0395_UART_BUF_SIZE - 1,
                              pdMS_TO_TICKS(100));

    return (len > 0);
}
