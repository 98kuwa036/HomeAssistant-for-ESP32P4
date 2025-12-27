/**
 * @file sen0540.c
 * @brief DFRobot SEN0540 Offline Voice Recognition Module Driver Implementation
 */

#include "sen0540.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SEN0540";

// Protocol constants
#define SEN0540_FRAME_HEADER        0xAA
#define SEN0540_FRAME_TAIL          0x55
#define SEN0540_RX_BUF_SIZE         256

// Command types
#define SEN0540_CMD_SET_WAKE_TIME   0x01
#define SEN0540_CMD_SET_MUTE        0x02
#define SEN0540_CMD_ADD_KEYWORD     0x03
#define SEN0540_CMD_DEL_KEYWORD     0x04
#define SEN0540_CMD_QUERY_STATUS    0x05

static uart_port_t s_uart_num = UART_NUM_2;
static bool s_initialized = false;
static sen0540_callback_t s_callback = NULL;
static void *s_callback_data = NULL;
static uint8_t s_rx_buffer[SEN0540_RX_BUF_SIZE];
static sen0540_result_t s_last_result = {0};

static uint8_t sen0540_calc_checksum(const uint8_t *data, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

static esp_err_t sen0540_send_command(uint8_t cmd, const uint8_t *data, size_t len)
{
    uint8_t frame[64];
    size_t frame_len = 0;

    frame[frame_len++] = SEN0540_FRAME_HEADER;
    frame[frame_len++] = (uint8_t)(len + 2);  // Length (cmd + data + checksum)
    frame[frame_len++] = cmd;

    if (data && len > 0) {
        memcpy(&frame[frame_len], data, len);
        frame_len += len;
    }

    frame[frame_len] = sen0540_calc_checksum(&frame[1], frame_len - 1);
    frame_len++;
    frame[frame_len++] = SEN0540_FRAME_TAIL;

    int written = uart_write_bytes(s_uart_num, frame, frame_len);
    return (written == frame_len) ? ESP_OK : ESP_FAIL;
}

static esp_err_t sen0540_parse_response(const uint8_t *data, size_t len, sen0540_result_t *result)
{
    if (len < 5) return ESP_ERR_INVALID_SIZE;

    // Verify frame format
    if (data[0] != SEN0540_FRAME_HEADER || data[len - 1] != SEN0540_FRAME_TAIL) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    uint8_t payload_len = data[1];
    if (payload_len > len - 4) return ESP_ERR_INVALID_SIZE;

    // Verify checksum
    uint8_t checksum = sen0540_calc_checksum(&data[1], payload_len);
    if (checksum != data[len - 2]) {
        return ESP_ERR_INVALID_CRC;
    }

    // Parse command ID
    result->cmd_id = data[2];
    result->confidence = (payload_len > 1) ? data[3] : 100;
    result->timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Get keyword name based on cmd_id (simplified)
    const char *keywords[] = {
        "Wake Word", "Light On", "Light Off", "Volume Up", "Volume Down",
        "Play", "Pause", "Next", "Previous"
    };
    if (result->cmd_id < sizeof(keywords) / sizeof(keywords[0])) {
        strncpy(result->keyword, keywords[result->cmd_id], sizeof(result->keyword) - 1);
    } else {
        snprintf(result->keyword, sizeof(result->keyword), "CMD_%d", result->cmd_id);
    }

    return ESP_OK;
}

esp_err_t sen0540_init(const sen0540_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    if (!config) return ESP_ERR_INVALID_ARG;

    s_uart_num = config->uart_num;
    s_callback = config->callback;
    s_callback_data = config->callback_data;

    // Configure UART
    uart_config_t uart_config = {
        .baud_rate = SEN0540_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_param_config(s_uart_num, &uart_config);
    if (ret != ESP_OK) return ret;

    ret = uart_set_pin(s_uart_num, config->tx_pin, config->rx_pin, -1, -1);
    if (ret != ESP_OK) return ret;

    ret = uart_driver_install(s_uart_num, SEN0540_RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    if (ret != ESP_OK) return ret;

    vTaskDelay(pdMS_TO_TICKS(100));

    // Set wake time
    if (config->wake_up_time_s > 0) {
        sen0540_set_wake_time(config->wake_up_time_s);
    }

    s_initialized = true;
    ESP_LOGI(TAG, "SEN0540 initialized on UART%d", s_uart_num);
    return ESP_OK;
}

esp_err_t sen0540_deinit(void)
{
    if (!s_initialized) return ESP_OK;

    uart_driver_delete(s_uart_num);
    s_initialized = false;
    ESP_LOGI(TAG, "SEN0540 deinitialized");
    return ESP_OK;
}

esp_err_t sen0540_process(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    int len = uart_read_bytes(s_uart_num, s_rx_buffer, SEN0540_RX_BUF_SIZE - 1, pdMS_TO_TICKS(10));
    if (len <= 0) return ESP_ERR_NOT_FOUND;

    sen0540_result_t result;
    esp_err_t ret = sen0540_parse_response(s_rx_buffer, len, &result);
    if (ret == ESP_OK) {
        memcpy(&s_last_result, &result, sizeof(sen0540_result_t));

        ESP_LOGI(TAG, "Voice command: %s (ID: %d, Confidence: %d%%)",
                 result.keyword, result.cmd_id, result.confidence);

        if (s_callback) {
            s_callback(result.cmd_id, result.keyword, s_callback_data);
        }
    }

    return ret;
}

esp_err_t sen0540_get_result(sen0540_result_t *result)
{
    if (!s_initialized || !result) return ESP_ERR_INVALID_ARG;

    // Process any pending data
    sen0540_process();

    memcpy(result, &s_last_result, sizeof(sen0540_result_t));
    return ESP_OK;
}

esp_err_t sen0540_add_keyword(uint8_t cmd_id, const char *keyword)
{
    if (!s_initialized || !keyword) return ESP_ERR_INVALID_ARG;

    uint8_t data[34];
    data[0] = cmd_id;
    size_t len = strlen(keyword);
    if (len > 32) len = 32;
    memcpy(&data[1], keyword, len);

    return sen0540_send_command(SEN0540_CMD_ADD_KEYWORD, data, len + 1);
}

esp_err_t sen0540_delete_keyword(uint8_t cmd_id)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    return sen0540_send_command(SEN0540_CMD_DEL_KEYWORD, &cmd_id, 1);
}

esp_err_t sen0540_set_wake_time(uint8_t seconds)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    return sen0540_send_command(SEN0540_CMD_SET_WAKE_TIME, &seconds, 1);
}

esp_err_t sen0540_set_mute(bool mute)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    uint8_t val = mute ? 1 : 0;
    return sen0540_send_command(SEN0540_CMD_SET_MUTE, &val, 1);
}

bool sen0540_is_connected(void)
{
    // Send status query and check for response
    sen0540_send_command(SEN0540_CMD_QUERY_STATUS, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    int len = uart_read_bytes(s_uart_num, s_rx_buffer, SEN0540_RX_BUF_SIZE - 1, pdMS_TO_TICKS(100));
    return (len > 0);
}
