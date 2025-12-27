/**
 * @file sen0428.c
 * @brief DFRobot SEN0428 PM2.5 Laser Dust Sensor Driver Implementation
 *
 * Based on Plantower PMSx003 protocol
 */

#include "sen0428.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SEN0428";

// Register addresses
#define SEN0428_REG_DATA            0x00

// Data frame length
#define SEN0428_FRAME_LEN           32

static i2c_port_t s_i2c_port = I2C_NUM_0;
static bool s_initialized = false;

static uint16_t sen0428_calc_checksum(const uint8_t *data, size_t len)
{
    uint16_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

esp_err_t sen0428_init(const sen0428_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    s_i2c_port = config ? config->i2c_port : I2C_NUM_0;

    if (!sen0428_is_connected()) {
        ESP_LOGE(TAG, "SEN0428 not found at address 0x%02X", SEN0428_I2C_ADDR);
        return ESP_ERR_NOT_FOUND;
    }

    // Wake up sensor
    sen0428_wakeup();
    vTaskDelay(pdMS_TO_TICKS(30000)); // Warm-up time (30s recommended)

    s_initialized = true;
    ESP_LOGI(TAG, "SEN0428 initialized");
    return ESP_OK;
}

esp_err_t sen0428_deinit(void)
{
    if (!s_initialized) return ESP_OK;
    sen0428_sleep();
    s_initialized = false;
    ESP_LOGI(TAG, "SEN0428 deinitialized");
    return ESP_OK;
}

esp_err_t sen0428_read_data(sen0428_data_t *data)
{
    if (!s_initialized || !data) return ESP_ERR_INVALID_ARG;

    memset(data, 0, sizeof(sen0428_data_t));

    // Read data frame
    uint8_t frame[SEN0428_FRAME_LEN];
    uint8_t reg = SEN0428_REG_DATA;

    esp_err_t ret = i2c_master_write_read_device(s_i2c_port, SEN0428_I2C_ADDR,
                                                  &reg, 1, frame, SEN0428_FRAME_LEN,
                                                  pdMS_TO_TICKS(100));
    if (ret != ESP_OK) return ret;

    // Verify start bytes (0x42 0x4D)
    if (frame[0] != 0x42 || frame[1] != 0x4D) {
        ESP_LOGE(TAG, "Invalid frame header: 0x%02X 0x%02X", frame[0], frame[1]);
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Verify checksum
    uint16_t frame_len = ((uint16_t)frame[2] << 8) | frame[3];
    if (frame_len > SEN0428_FRAME_LEN - 4) {
        ESP_LOGE(TAG, "Invalid frame length: %d", frame_len);
        return ESP_ERR_INVALID_SIZE;
    }

    uint16_t checksum = ((uint16_t)frame[SEN0428_FRAME_LEN - 2] << 8) | frame[SEN0428_FRAME_LEN - 1];
    uint16_t calc_sum = sen0428_calc_checksum(frame, SEN0428_FRAME_LEN - 2);
    if (checksum != calc_sum) {
        ESP_LOGE(TAG, "Checksum mismatch: 0x%04X vs 0x%04X", checksum, calc_sum);
        return ESP_ERR_INVALID_CRC;
    }

    // Parse data (big-endian)
    data->pm1_0_std = ((uint16_t)frame[4] << 8) | frame[5];
    data->pm2_5_std = ((uint16_t)frame[6] << 8) | frame[7];
    data->pm10_std = ((uint16_t)frame[8] << 8) | frame[9];

    data->pm1_0_atm = ((uint16_t)frame[10] << 8) | frame[11];
    data->pm2_5_atm = ((uint16_t)frame[12] << 8) | frame[13];
    data->pm10_atm = ((uint16_t)frame[14] << 8) | frame[15];

    data->particles_0_3um = ((uint16_t)frame[16] << 8) | frame[17];
    data->particles_0_5um = ((uint16_t)frame[18] << 8) | frame[19];
    data->particles_1_0um = ((uint16_t)frame[20] << 8) | frame[21];
    data->particles_2_5um = ((uint16_t)frame[22] << 8) | frame[23];
    data->particles_5_0um = ((uint16_t)frame[24] << 8) | frame[25];
    data->particles_10um = ((uint16_t)frame[26] << 8) | frame[27];

    data->timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    return ESP_OK;
}

esp_err_t sen0428_sleep(void)
{
    // Sleep command: 0x42 0x4D 0xE4 0x00 0x00 0x01 0x73
    uint8_t cmd[] = {0x42, 0x4D, 0xE4, 0x00, 0x00, 0x01, 0x73};
    return i2c_master_write_to_device(s_i2c_port, SEN0428_I2C_ADDR, cmd, sizeof(cmd), pdMS_TO_TICKS(100));
}

esp_err_t sen0428_wakeup(void)
{
    // Wakeup command: 0x42 0x4D 0xE4 0x00 0x01 0x01 0x74
    uint8_t cmd[] = {0x42, 0x4D, 0xE4, 0x00, 0x01, 0x01, 0x74};
    return i2c_master_write_to_device(s_i2c_port, SEN0428_I2C_ADDR, cmd, sizeof(cmd), pdMS_TO_TICKS(100));
}

bool sen0428_is_connected(void)
{
    uint8_t dummy;
    return i2c_master_read_from_device(s_i2c_port, SEN0428_I2C_ADDR, &dummy, 1, pdMS_TO_TICKS(50)) == ESP_OK;
}
