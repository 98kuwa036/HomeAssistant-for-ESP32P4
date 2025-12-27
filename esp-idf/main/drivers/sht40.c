/**
 * @file sht40.c
 * @brief Sensirion SHT40 Temperature/Humidity Sensor Driver Implementation
 */

#include "sht40.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SHT40";

// Command codes
#define SHT40_CMD_MEASURE_HIGH      0xFD
#define SHT40_CMD_MEASURE_MEDIUM    0xF6
#define SHT40_CMD_MEASURE_LOW       0xE0
#define SHT40_CMD_READ_SERIAL       0x89
#define SHT40_CMD_SOFT_RESET        0x94
#define SHT40_CMD_HEATER_HIGH_1S    0x39
#define SHT40_CMD_HEATER_HIGH_100MS 0x32
#define SHT40_CMD_HEATER_MED_1S     0x2F
#define SHT40_CMD_HEATER_MED_100MS  0x24
#define SHT40_CMD_HEATER_LOW_1S     0x1E
#define SHT40_CMD_HEATER_LOW_100MS  0x15

static i2c_port_t s_i2c_port = I2C_NUM_0;
static bool s_initialized = false;
static sht40_precision_t s_precision = SHT40_PRECISION_HIGH;

static uint8_t sht40_calc_crc(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x31;
            else crc <<= 1;
        }
    }
    return crc;
}

static esp_err_t sht40_send_command(uint8_t cmd)
{
    return i2c_master_write_to_device(s_i2c_port, SHT40_I2C_ADDR, &cmd, 1, pdMS_TO_TICKS(100));
}

esp_err_t sht40_init(const sht40_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    s_i2c_port = config ? config->i2c_port : I2C_NUM_0;
    s_precision = config ? config->precision : SHT40_PRECISION_HIGH;

    if (!sht40_is_connected()) {
        ESP_LOGE(TAG, "SHT40 not found at address 0x%02X", SHT40_I2C_ADDR);
        return ESP_ERR_NOT_FOUND;
    }

    // Soft reset
    sht40_soft_reset();

    s_initialized = true;
    ESP_LOGI(TAG, "SHT40 initialized");
    return ESP_OK;
}

esp_err_t sht40_deinit(void)
{
    if (!s_initialized) return ESP_OK;
    s_initialized = false;
    ESP_LOGI(TAG, "SHT40 deinitialized");
    return ESP_OK;
}

esp_err_t sht40_read_data(sht40_data_t *data)
{
    if (!s_initialized || !data) return ESP_ERR_INVALID_ARG;

    memset(data, 0, sizeof(sht40_data_t));

    // Select command based on precision
    uint8_t cmd;
    uint16_t delay_ms;
    switch (s_precision) {
        case SHT40_PRECISION_HIGH:
            cmd = SHT40_CMD_MEASURE_HIGH;
            delay_ms = 10;
            break;
        case SHT40_PRECISION_MEDIUM:
            cmd = SHT40_CMD_MEASURE_MEDIUM;
            delay_ms = 5;
            break;
        case SHT40_PRECISION_LOW:
        default:
            cmd = SHT40_CMD_MEASURE_LOW;
            delay_ms = 2;
            break;
    }

    esp_err_t ret = sht40_send_command(cmd);
    if (ret != ESP_OK) return ret;

    vTaskDelay(pdMS_TO_TICKS(delay_ms));

    // Read 6 bytes: temp[2] + crc + humidity[2] + crc
    uint8_t buf[6];
    ret = i2c_master_read_from_device(s_i2c_port, SHT40_I2C_ADDR, buf, 6, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) return ret;

    // Verify CRCs
    if (sht40_calc_crc(buf, 2) != buf[2] || sht40_calc_crc(&buf[3], 2) != buf[5]) {
        ESP_LOGE(TAG, "CRC mismatch");
        return ESP_ERR_INVALID_CRC;
    }

    // Convert raw values
    uint16_t raw_temp = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t raw_hum = ((uint16_t)buf[3] << 8) | buf[4];

    data->temperature = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
    data->humidity = -6.0f + 125.0f * ((float)raw_hum / 65535.0f);

    // Clamp humidity to valid range
    if (data->humidity < 0) data->humidity = 0;
    if (data->humidity > 100) data->humidity = 100;

    data->timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    return ESP_OK;
}

esp_err_t sht40_activate_heater(uint16_t duration_ms)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    uint8_t cmd;
    if (duration_ms >= 1000) {
        cmd = SHT40_CMD_HEATER_HIGH_1S;
    } else {
        cmd = SHT40_CMD_HEATER_HIGH_100MS;
    }

    esp_err_t ret = sht40_send_command(cmd);
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(duration_ms >= 1000 ? 1100 : 110));
    }
    return ret;
}

esp_err_t sht40_soft_reset(void)
{
    esp_err_t ret = sht40_send_command(SHT40_CMD_SOFT_RESET);
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return ret;
}

esp_err_t sht40_get_serial(uint32_t *serial)
{
    if (!serial) return ESP_ERR_INVALID_ARG;

    esp_err_t ret = sht40_send_command(SHT40_CMD_READ_SERIAL);
    if (ret != ESP_OK) return ret;

    vTaskDelay(pdMS_TO_TICKS(1));

    uint8_t buf[6];
    ret = i2c_master_read_from_device(s_i2c_port, SHT40_I2C_ADDR, buf, 6, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) return ret;

    *serial = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
              ((uint32_t)buf[3] << 8) | buf[4];
    return ESP_OK;
}

bool sht40_is_connected(void)
{
    uint8_t dummy;
    return i2c_master_read_from_device(s_i2c_port, SHT40_I2C_ADDR, &dummy, 1, pdMS_TO_TICKS(50)) == ESP_OK;
}
