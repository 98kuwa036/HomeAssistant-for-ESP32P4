/**
 * @file scd41.c
 * @brief Sensirion SCD41 CO2/Temperature/Humidity Sensor Driver Implementation
 */

#include "scd41.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SCD41";

// Command codes
#define SCD41_CMD_START_PERIODIC        0x21B1
#define SCD41_CMD_START_LOW_POWER       0x21AC
#define SCD41_CMD_SINGLE_SHOT           0x219D
#define SCD41_CMD_STOP_PERIODIC         0x3F86
#define SCD41_CMD_READ_MEASUREMENT      0xEC05
#define SCD41_CMD_DATA_READY            0xE4B8
#define SCD41_CMD_SET_ALTITUDE          0x2427
#define SCD41_CMD_SET_TEMP_OFFSET       0x241D
#define SCD41_CMD_FORCE_RECAL           0x362F
#define SCD41_CMD_SET_ASC               0x2416
#define SCD41_CMD_GET_SERIAL            0x3682
#define SCD41_CMD_FACTORY_RESET         0x3632
#define SCD41_CMD_REINIT                0x3646
#define SCD41_CMD_PERSIST_SETTINGS      0x3615

static i2c_port_t s_i2c_port = I2C_NUM_0;
static bool s_initialized = false;
static scd41_config_t s_config;

static uint8_t scd41_calc_crc(const uint8_t *data, size_t len)
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

static esp_err_t scd41_send_command(uint16_t cmd)
{
    uint8_t data[2] = { (uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF) };
    return i2c_master_write_to_device(s_i2c_port, SCD41_I2C_ADDR, data, 2, pdMS_TO_TICKS(100));
}

static esp_err_t scd41_send_command_with_data(uint16_t cmd, uint16_t value)
{
    uint8_t data[5];
    data[0] = (uint8_t)(cmd >> 8);
    data[1] = (uint8_t)(cmd & 0xFF);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)(value & 0xFF);
    data[4] = scd41_calc_crc(&data[2], 2);
    return i2c_master_write_to_device(s_i2c_port, SCD41_I2C_ADDR, data, 5, pdMS_TO_TICKS(100));
}

static esp_err_t scd41_read_words(uint16_t cmd, uint16_t *words, size_t num_words, uint16_t delay_ms)
{
    esp_err_t ret = scd41_send_command(cmd);
    if (ret != ESP_OK) return ret;

    vTaskDelay(pdMS_TO_TICKS(delay_ms));

    size_t read_len = num_words * 3;
    uint8_t *buffer = malloc(read_len);
    if (!buffer) return ESP_ERR_NO_MEM;

    ret = i2c_master_read_from_device(s_i2c_port, SCD41_I2C_ADDR, buffer, read_len, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        free(buffer);
        return ret;
    }

    for (size_t i = 0; i < num_words; i++) {
        uint8_t *word_data = &buffer[i * 3];
        if (scd41_calc_crc(word_data, 2) != word_data[2]) {
            free(buffer);
            return ESP_ERR_INVALID_CRC;
        }
        words[i] = ((uint16_t)word_data[0] << 8) | word_data[1];
    }

    free(buffer);
    return ESP_OK;
}

esp_err_t scd41_init(const scd41_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    if (!config) {
        scd41_config_t default_config = SCD41_DEFAULT_CONFIG();
        memcpy(&s_config, &default_config, sizeof(scd41_config_t));
    } else {
        memcpy(&s_config, config, sizeof(scd41_config_t));
    }
    s_i2c_port = s_config.i2c_port;

    if (!scd41_is_connected()) {
        ESP_LOGE(TAG, "SCD41 not found at address 0x%02X", SCD41_I2C_ADDR);
        return ESP_ERR_NOT_FOUND;
    }

    // Stop any existing measurement
    scd41_send_command(SCD41_CMD_STOP_PERIODIC);
    vTaskDelay(pdMS_TO_TICKS(500));

    // Set altitude compensation
    if (s_config.altitude_m > 0) {
        scd41_set_altitude(s_config.altitude_m);
    }

    s_initialized = true;
    ESP_LOGI(TAG, "SCD41 initialized");
    return ESP_OK;
}

esp_err_t scd41_deinit(void)
{
    if (!s_initialized) return ESP_OK;
    scd41_stop_measurement();
    s_initialized = false;
    ESP_LOGI(TAG, "SCD41 deinitialized");
    return ESP_OK;
}

esp_err_t scd41_start_measurement(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    uint16_t cmd = (s_config.mode == SCD41_MODE_LOW_POWER) ?
                   SCD41_CMD_START_LOW_POWER : SCD41_CMD_START_PERIODIC;

    esp_err_t ret = scd41_send_command(cmd);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Measurement started (mode: %s)",
                 s_config.mode == SCD41_MODE_LOW_POWER ? "low power" : "periodic");
    }
    return ret;
}

esp_err_t scd41_stop_measurement(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    return scd41_send_command(SCD41_CMD_STOP_PERIODIC);
}

bool scd41_is_data_ready(void)
{
    if (!s_initialized) return false;

    uint16_t status;
    if (scd41_read_words(SCD41_CMD_DATA_READY, &status, 1, 1) != ESP_OK) {
        return false;
    }
    return (status & 0x07FF) != 0;
}

esp_err_t scd41_read_data(scd41_data_t *data)
{
    if (!s_initialized || !data) return ESP_ERR_INVALID_ARG;

    memset(data, 0, sizeof(scd41_data_t));

    uint16_t raw[3];
    esp_err_t ret = scd41_read_words(SCD41_CMD_READ_MEASUREMENT, raw, 3, 1);
    if (ret != ESP_OK) return ret;

    data->co2_ppm = raw[0];
    data->temperature = -45.0f + 175.0f * ((float)raw[1] / 65535.0f);
    data->humidity = 100.0f * ((float)raw[2] / 65535.0f);
    data->data_valid = (data->co2_ppm >= 400 && data->co2_ppm <= 5000);
    data->timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    return ESP_OK;
}

esp_err_t scd41_set_altitude(uint16_t altitude_m)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    esp_err_t ret = scd41_send_command_with_data(SCD41_CMD_SET_ALTITUDE, altitude_m);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Altitude set to %d m", altitude_m);
    }
    return ret;
}

esp_err_t scd41_set_temperature_offset(float offset)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    uint16_t offset_ticks = (uint16_t)(offset * 65535.0f / 175.0f);
    return scd41_send_command_with_data(SCD41_CMD_SET_TEMP_OFFSET, offset_ticks);
}

esp_err_t scd41_force_recalibration(uint16_t target_co2_ppm)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    return scd41_send_command_with_data(SCD41_CMD_FORCE_RECAL, target_co2_ppm);
}

esp_err_t scd41_get_serial_number(uint64_t *serial)
{
    if (!serial) return ESP_ERR_INVALID_ARG;

    uint16_t words[3];
    esp_err_t ret = scd41_read_words(SCD41_CMD_GET_SERIAL, words, 3, 1);
    if (ret != ESP_OK) return ret;

    *serial = ((uint64_t)words[0] << 32) | ((uint64_t)words[1] << 16) | words[2];
    return ESP_OK;
}

bool scd41_is_connected(void)
{
    uint8_t dummy;
    return i2c_master_read_from_device(s_i2c_port, SCD41_I2C_ADDR, &dummy, 1, pdMS_TO_TICKS(50)) == ESP_OK;
}
