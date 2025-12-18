/**
 * @file sen66.c
 * @brief SEN66 Environmental Sensor Node Driver Implementation
 */

#include "sen66.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SEN66";

static i2c_port_t s_i2c_port = I2C_NUM_0;
static bool s_initialized = false;

/**
 * @brief Calculate CRC-8 for Sensirion sensors
 */
static uint8_t sen66_calc_crc(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief Send command to SEN66
 */
static esp_err_t sen66_send_command(uint16_t cmd)
{
    uint8_t data[2] = {
        (uint8_t)(cmd >> 8),
        (uint8_t)(cmd & 0xFF)
    };

    esp_err_t ret = i2c_master_write_to_device(
        s_i2c_port,
        SEN66_I2C_ADDR,
        data,
        sizeof(data),
        pdMS_TO_TICKS(100)
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send command 0x%04X: %s", cmd, esp_err_to_name(ret));
    }

    return ret;
}

/**
 * @brief Read data from SEN66 with CRC verification
 */
static esp_err_t sen66_read_words(uint16_t cmd, uint16_t *words, size_t num_words)
{
    // Send command
    esp_err_t ret = sen66_send_command(cmd);
    if (ret != ESP_OK) {
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(SEN66_COMMAND_DELAY_MS));

    // Read data (each word is 2 bytes + 1 CRC byte)
    size_t read_len = num_words * 3;
    uint8_t *buffer = malloc(read_len);
    if (!buffer) {
        return ESP_ERR_NO_MEM;
    }

    ret = i2c_master_read_from_device(
        s_i2c_port,
        SEN66_I2C_ADDR,
        buffer,
        read_len,
        pdMS_TO_TICKS(100)
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read data: %s", esp_err_to_name(ret));
        free(buffer);
        return ret;
    }

    // Parse and verify CRC
    for (size_t i = 0; i < num_words; i++) {
        uint8_t *word_data = &buffer[i * 3];
        uint8_t crc = sen66_calc_crc(word_data, 2);

        if (crc != word_data[2]) {
            ESP_LOGE(TAG, "CRC mismatch for word %zu: got 0x%02X, expected 0x%02X",
                     i, word_data[2], crc);
            free(buffer);
            return ESP_ERR_INVALID_CRC;
        }

        words[i] = (word_data[0] << 8) | word_data[1];
    }

    free(buffer);
    return ESP_OK;
}

esp_err_t sen66_init(i2c_port_t i2c_port)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    s_i2c_port = i2c_port;

    // Check if device is present
    if (!sen66_is_connected()) {
        ESP_LOGE(TAG, "SEN66 not found at address 0x%02X", SEN66_I2C_ADDR);
        return ESP_ERR_NOT_FOUND;
    }

    // Read and log device info
    sen66_device_info_t info;
    if (sen66_get_device_info(&info) == ESP_OK) {
        ESP_LOGI(TAG, "Found %s (SN: %s) FW: %d.%d",
                 info.product_name,
                 info.serial_number,
                 info.firmware_major,
                 info.firmware_minor);
    }

    s_initialized = true;
    ESP_LOGI(TAG, "SEN66 initialized successfully");

    return ESP_OK;
}

esp_err_t sen66_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    sen66_stop_measurement();
    s_initialized = false;

    ESP_LOGI(TAG, "SEN66 deinitialized");
    return ESP_OK;
}

esp_err_t sen66_start_measurement(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = sen66_send_command(SEN66_CMD_START_MEASUREMENT);
    if (ret == ESP_OK) {
        // Wait for sensor to start
        vTaskDelay(pdMS_TO_TICKS(SEN66_STARTUP_TIME_MS));
        ESP_LOGI(TAG, "Measurement started");
    }

    return ret;
}

esp_err_t sen66_stop_measurement(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = sen66_send_command(SEN66_CMD_STOP_MEASUREMENT);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Measurement stopped");
    }

    return ret;
}

esp_err_t sen66_is_data_ready(bool *ready)
{
    if (!s_initialized || !ready) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t status;
    esp_err_t ret = sen66_read_words(SEN66_CMD_READ_DATA_READY, &status, 1);

    if (ret == ESP_OK) {
        *ready = (status & 0x0001) != 0;
    }

    return ret;
}

esp_err_t sen66_read_data(sen66_data_t *data)
{
    if (!s_initialized || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(data, 0, sizeof(sen66_data_t));

    // SEN66 returns 9 words of data
    uint16_t raw[9];
    esp_err_t ret = sen66_read_words(SEN66_CMD_READ_MEASURED_VALUES, raw, 9);

    if (ret != ESP_OK) {
        return ret;
    }

    // Parse raw values (scaling per datasheet)
    // PM values are scaled by 10
    data->pm1_0 = (int16_t)raw[0] / 10.0f;
    data->pm2_5 = (int16_t)raw[1] / 10.0f;
    data->pm4_0 = (int16_t)raw[2] / 10.0f;
    data->pm10 = (int16_t)raw[3] / 10.0f;

    // Humidity scaled by 100
    data->humidity = (int16_t)raw[4] / 100.0f;

    // Temperature scaled by 200
    data->temperature = (int16_t)raw[5] / 200.0f;

    // VOC and NOx indices scaled by 10
    data->voc_index = (int16_t)raw[6] / 10.0f;
    data->nox_index = (int16_t)raw[7] / 10.0f;

    // CO2 is raw ppm value
    data->co2 = (float)raw[8];

    // Validate data ranges
    data->data_valid = (data->pm2_5 >= 0 && data->pm2_5 < 1000) &&
                       (data->co2 >= 400 && data->co2 < 10000) &&
                       (data->humidity >= 0 && data->humidity <= 100) &&
                       (data->temperature >= -40 && data->temperature <= 85);

    return ESP_OK;
}

esp_err_t sen66_get_device_info(sen66_device_info_t *info)
{
    if (!info) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(info, 0, sizeof(sen66_device_info_t));

    // Read product name (16 words = 32 chars)
    uint16_t name_words[16];
    esp_err_t ret = sen66_read_words(SEN66_CMD_READ_PRODUCT_NAME, name_words, 16);
    if (ret == ESP_OK) {
        for (int i = 0; i < 16; i++) {
            info->product_name[i * 2] = (char)(name_words[i] >> 8);
            info->product_name[i * 2 + 1] = (char)(name_words[i] & 0xFF);
        }
        info->product_name[31] = '\0';
    }

    // Read serial number (16 words = 32 chars)
    uint16_t sn_words[16];
    ret = sen66_read_words(SEN66_CMD_READ_SERIAL_NUMBER, sn_words, 16);
    if (ret == ESP_OK) {
        for (int i = 0; i < 16; i++) {
            info->serial_number[i * 2] = (char)(sn_words[i] >> 8);
            info->serial_number[i * 2 + 1] = (char)(sn_words[i] & 0xFF);
        }
        info->serial_number[31] = '\0';
    }

    // Read firmware version
    uint16_t fw_version;
    ret = sen66_read_words(SEN66_CMD_READ_FIRMWARE_VERSION, &fw_version, 1);
    if (ret == ESP_OK) {
        info->firmware_major = (fw_version >> 8) & 0xFF;
        info->firmware_minor = fw_version & 0xFF;
    }

    return ESP_OK;
}

esp_err_t sen66_reset(void)
{
    esp_err_t ret = sen66_send_command(SEN66_CMD_DEVICE_RESET);
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(100));
        ESP_LOGI(TAG, "Device reset");
    }
    return ret;
}

bool sen66_is_connected(void)
{
    uint8_t dummy;
    esp_err_t ret = i2c_master_read_from_device(
        s_i2c_port,
        SEN66_I2C_ADDR,
        &dummy,
        1,
        pdMS_TO_TICKS(50)
    );
    return (ret == ESP_OK);
}
