/**
 * @file sgp40.c
 * @brief Sensirion SGP40 VOC Sensor Driver Implementation
 *
 * Includes simplified VOC Index Algorithm implementation
 */

#include "sgp40.h"
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SGP40";

#define SGP40_CMD_MEASURE_RAW       0x260F
#define SGP40_CMD_HEATER_OFF        0x3615
#define SGP40_CMD_SOFT_RESET        0x0006

static i2c_port_t s_i2c_port = I2C_NUM_0;
static bool s_initialized = false;

// Simple VOC Index algorithm state
static float s_voc_mean = 0;
static float s_voc_variance = 0;
static int s_voc_sample_count = 0;
static const int VOC_LEARNING_SAMPLES = 100;

static uint8_t sgp40_calc_crc(const uint8_t *data, size_t len)
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

static uint16_t humidity_to_ticks(float humidity)
{
    if (humidity < 0) humidity = 0;
    if (humidity > 100) humidity = 100;
    return (uint16_t)(humidity * 65535.0f / 100.0f);
}

static uint16_t temperature_to_ticks(float temperature)
{
    if (temperature < -45) temperature = -45;
    if (temperature > 130) temperature = 130;
    return (uint16_t)((temperature + 45.0f) * 65535.0f / 175.0f);
}

static int32_t calculate_voc_index(uint16_t raw_signal)
{
    float raw = (float)raw_signal;

    // Learning phase
    if (s_voc_sample_count < VOC_LEARNING_SAMPLES) {
        s_voc_sample_count++;
        float delta = raw - s_voc_mean;
        s_voc_mean += delta / s_voc_sample_count;
        s_voc_variance += delta * (raw - s_voc_mean);
        return 100; // Return neutral during learning
    }

    // Calculate VOC index
    float std_dev = sqrtf(s_voc_variance / (VOC_LEARNING_SAMPLES - 1));
    if (std_dev < 1.0f) std_dev = 1.0f;

    float z_score = (raw - s_voc_mean) / std_dev;
    int32_t voc_index = 100 + (int32_t)(z_score * 100.0f);

    // Clamp to valid range
    if (voc_index < 1) voc_index = 1;
    if (voc_index > 500) voc_index = 500;

    // Slowly adapt the mean
    s_voc_mean = s_voc_mean * 0.999f + raw * 0.001f;

    return voc_index;
}

esp_err_t sgp40_init(const sgp40_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    s_i2c_port = config ? config->i2c_port : I2C_NUM_0;

    if (!sgp40_is_connected()) {
        ESP_LOGE(TAG, "SGP40 not found at address 0x%02X", SGP40_I2C_ADDR);
        return ESP_ERR_NOT_FOUND;
    }

    // Reset VOC algorithm state
    s_voc_mean = 0;
    s_voc_variance = 0;
    s_voc_sample_count = 0;

    s_initialized = true;
    ESP_LOGI(TAG, "SGP40 initialized");
    return ESP_OK;
}

esp_err_t sgp40_deinit(void)
{
    if (!s_initialized) return ESP_OK;
    sgp40_heater_off();
    s_initialized = false;
    ESP_LOGI(TAG, "SGP40 deinitialized");
    return ESP_OK;
}

esp_err_t sgp40_measure_raw(uint16_t *raw_signal, float humidity, float temperature)
{
    if (!s_initialized || !raw_signal) return ESP_ERR_INVALID_ARG;

    uint16_t hum_ticks = humidity_to_ticks(humidity);
    uint16_t temp_ticks = temperature_to_ticks(temperature);

    uint8_t cmd[8];
    cmd[0] = (SGP40_CMD_MEASURE_RAW >> 8) & 0xFF;
    cmd[1] = SGP40_CMD_MEASURE_RAW & 0xFF;
    cmd[2] = (hum_ticks >> 8) & 0xFF;
    cmd[3] = hum_ticks & 0xFF;
    cmd[4] = sgp40_calc_crc(&cmd[2], 2);
    cmd[5] = (temp_ticks >> 8) & 0xFF;
    cmd[6] = temp_ticks & 0xFF;
    cmd[7] = sgp40_calc_crc(&cmd[5], 2);

    esp_err_t ret = i2c_master_write_to_device(s_i2c_port, SGP40_I2C_ADDR, cmd, 8, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) return ret;

    vTaskDelay(pdMS_TO_TICKS(30)); // Measurement time

    uint8_t response[3];
    ret = i2c_master_read_from_device(s_i2c_port, SGP40_I2C_ADDR, response, 3, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) return ret;

    if (sgp40_calc_crc(response, 2) != response[2]) {
        return ESP_ERR_INVALID_CRC;
    }

    *raw_signal = ((uint16_t)response[0] << 8) | response[1];
    return ESP_OK;
}

esp_err_t sgp40_read_voc_index(sgp40_data_t *data, float humidity, float temperature)
{
    if (!s_initialized || !data) return ESP_ERR_INVALID_ARG;

    memset(data, 0, sizeof(sgp40_data_t));

    esp_err_t ret = sgp40_measure_raw(&data->voc_raw, humidity, temperature);
    if (ret != ESP_OK) return ret;

    data->voc_index = calculate_voc_index(data->voc_raw);
    data->timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    return ESP_OK;
}

esp_err_t sgp40_heater_off(void)
{
    uint8_t cmd[2] = { (SGP40_CMD_HEATER_OFF >> 8) & 0xFF, SGP40_CMD_HEATER_OFF & 0xFF };
    return i2c_master_write_to_device(s_i2c_port, SGP40_I2C_ADDR, cmd, 2, pdMS_TO_TICKS(100));
}

bool sgp40_is_connected(void)
{
    uint8_t dummy;
    return i2c_master_read_from_device(s_i2c_port, SGP40_I2C_ADDR, &dummy, 1, pdMS_TO_TICKS(50)) == ESP_OK;
}
