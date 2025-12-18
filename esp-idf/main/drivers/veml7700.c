/**
 * @file veml7700.c
 * @brief VEML7700 Ambient Light Sensor Driver Implementation
 */

#include "veml7700.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "VEML7700";

// Register addresses
#define VEML7700_REG_ALS_CONF       0x00
#define VEML7700_REG_ALS_WH         0x01    // High threshold
#define VEML7700_REG_ALS_WL         0x02    // Low threshold
#define VEML7700_REG_PWR_SAVE       0x03    // Power saving mode
#define VEML7700_REG_ALS            0x04    // ALS data output
#define VEML7700_REG_WHITE          0x05    // White channel data
#define VEML7700_REG_ALS_INT        0x06    // Interrupt status

// Configuration bits
#define VEML7700_CONF_ALS_SD        0x01    // Shutdown bit
#define VEML7700_CONF_ALS_INT_EN    0x02    // Interrupt enable

static i2c_port_t s_i2c_port = I2C_NUM_0;
static bool s_initialized = false;
static veml7700_config_t s_config;

// Resolution table (lux/count) based on gain and integration time
static const float resolution_table[4][6] = {
    // IT:   25ms    50ms   100ms   200ms   400ms   800ms
    /*1x*/  {0.2304, 0.1152, 0.0576, 0.0288, 0.0144, 0.0072},
    /*2x*/  {0.1152, 0.0576, 0.0288, 0.0144, 0.0072, 0.0036},
    /*1/8*/ {1.8432, 0.9216, 0.4608, 0.2304, 0.1152, 0.0576},
    /*1/4*/ {0.9216, 0.4608, 0.2304, 0.1152, 0.0576, 0.0288},
};

/**
 * @brief Write 16-bit register
 */
static esp_err_t veml7700_write_reg(uint8_t reg, uint16_t value)
{
    uint8_t buf[3] = {reg, (uint8_t)(value & 0xFF), (uint8_t)(value >> 8)};
    return i2c_master_write_to_device(s_i2c_port, VEML7700_I2C_ADDR,
                                       buf, 3, pdMS_TO_TICKS(100));
}

/**
 * @brief Read 16-bit register
 */
static esp_err_t veml7700_read_reg(uint8_t reg, uint16_t *value)
{
    uint8_t buf[2];
    esp_err_t ret = i2c_master_write_read_device(s_i2c_port, VEML7700_I2C_ADDR,
                                                  &reg, 1, buf, 2, pdMS_TO_TICKS(100));
    if (ret == ESP_OK) {
        *value = ((uint16_t)buf[1] << 8) | buf[0];
    }
    return ret;
}

/**
 * @brief Get resolution based on current settings
 */
static float veml7700_get_resolution(void)
{
    int gain_idx;
    int it_idx;

    switch (s_config.gain) {
        case VEML7700_GAIN_1X:   gain_idx = 0; break;
        case VEML7700_GAIN_2X:   gain_idx = 1; break;
        case VEML7700_GAIN_1_8X: gain_idx = 2; break;
        case VEML7700_GAIN_1_4X: gain_idx = 3; break;
        default: gain_idx = 0;
    }

    switch (s_config.integration_time) {
        case VEML7700_IT_25MS:  it_idx = 0; break;
        case VEML7700_IT_50MS:  it_idx = 1; break;
        case VEML7700_IT_100MS: it_idx = 2; break;
        case VEML7700_IT_200MS: it_idx = 3; break;
        case VEML7700_IT_400MS: it_idx = 4; break;
        case VEML7700_IT_800MS: it_idx = 5; break;
        default: it_idx = 2;
    }

    return resolution_table[gain_idx][it_idx];
}

/**
 * @brief Update configuration register
 */
static esp_err_t veml7700_update_config(void)
{
    uint16_t conf = 0;

    // Set gain (bits 12:11)
    conf |= ((uint16_t)s_config.gain & 0x03) << 11;

    // Set integration time (bits 9:6)
    conf |= ((uint16_t)s_config.integration_time & 0x0F) << 6;

    return veml7700_write_reg(VEML7700_REG_ALS_CONF, conf);
}

esp_err_t veml7700_init(const veml7700_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    s_i2c_port = config->i2c_port;
    memcpy(&s_config, config, sizeof(veml7700_config_t));

    // Check connection
    if (!veml7700_is_connected()) {
        ESP_LOGE(TAG, "VEML7700 not found at address 0x%02X", VEML7700_I2C_ADDR);
        return ESP_ERR_NOT_FOUND;
    }

    // Power on and configure
    esp_err_t ret = veml7700_update_config();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure sensor");
        return ret;
    }

    // Disable power saving mode
    ret = veml7700_write_reg(VEML7700_REG_PWR_SAVE, 0x0000);
    if (ret != ESP_OK) {
        return ret;
    }

    // Wait for first measurement
    vTaskDelay(pdMS_TO_TICKS(150));

    s_initialized = true;
    ESP_LOGI(TAG, "VEML7700 initialized (Gain: %d, IT: %dms)",
             config->gain, config->integration_time);

    return ESP_OK;
}

esp_err_t veml7700_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    veml7700_power_off();
    s_initialized = false;

    ESP_LOGI(TAG, "VEML7700 deinitialized");
    return ESP_OK;
}

esp_err_t veml7700_power_on(void)
{
    uint16_t conf;
    esp_err_t ret = veml7700_read_reg(VEML7700_REG_ALS_CONF, &conf);
    if (ret != ESP_OK) {
        return ret;
    }

    conf &= ~VEML7700_CONF_ALS_SD;  // Clear shutdown bit
    return veml7700_write_reg(VEML7700_REG_ALS_CONF, conf);
}

esp_err_t veml7700_power_off(void)
{
    uint16_t conf;
    esp_err_t ret = veml7700_read_reg(VEML7700_REG_ALS_CONF, &conf);
    if (ret != ESP_OK) {
        return ret;
    }

    conf |= VEML7700_CONF_ALS_SD;  // Set shutdown bit
    return veml7700_write_reg(VEML7700_REG_ALS_CONF, conf);
}

esp_err_t veml7700_read_data(veml7700_data_t *data)
{
    if (!s_initialized || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(data, 0, sizeof(veml7700_data_t));

    // Read ALS channel
    esp_err_t ret = veml7700_read_reg(VEML7700_REG_ALS, &data->als_raw);
    if (ret != ESP_OK) {
        return ret;
    }

    // Read white channel
    ret = veml7700_read_reg(VEML7700_REG_WHITE, &data->white_raw);
    if (ret != ESP_OK) {
        return ret;
    }

    // Calculate lux
    float resolution = veml7700_get_resolution();
    float lux_raw = data->als_raw * resolution;

    // Apply correction for high lux values (per datasheet)
    if (lux_raw > 1000.0f) {
        lux_raw = 6.0135e-13f * lux_raw * lux_raw * lux_raw * lux_raw
                - 9.3924e-9f * lux_raw * lux_raw * lux_raw
                + 8.1488e-5f * lux_raw * lux_raw
                + 1.0023f * lux_raw;
    }

    data->lux = (uint32_t)lux_raw;
    data->timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Auto-gain adjustment
    if (s_config.auto_gain) {
        if (data->als_raw > 60000 && s_config.gain != VEML7700_GAIN_1_8X) {
            // Too bright, reduce gain
            if (s_config.gain == VEML7700_GAIN_2X) {
                veml7700_set_gain(VEML7700_GAIN_1X);
            } else if (s_config.gain == VEML7700_GAIN_1X) {
                veml7700_set_gain(VEML7700_GAIN_1_4X);
            } else if (s_config.gain == VEML7700_GAIN_1_4X) {
                veml7700_set_gain(VEML7700_GAIN_1_8X);
            }
        } else if (data->als_raw < 100 && s_config.gain != VEML7700_GAIN_2X) {
            // Too dark, increase gain
            if (s_config.gain == VEML7700_GAIN_1_8X) {
                veml7700_set_gain(VEML7700_GAIN_1_4X);
            } else if (s_config.gain == VEML7700_GAIN_1_4X) {
                veml7700_set_gain(VEML7700_GAIN_1X);
            } else if (s_config.gain == VEML7700_GAIN_1X) {
                veml7700_set_gain(VEML7700_GAIN_2X);
            }
        }
    }

    return ESP_OK;
}

esp_err_t veml7700_read_lux(uint32_t *lux)
{
    veml7700_data_t data;
    esp_err_t ret = veml7700_read_data(&data);
    if (ret == ESP_OK) {
        *lux = data.lux;
    }
    return ret;
}

esp_err_t veml7700_set_gain(veml7700_gain_t gain)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_config.gain = gain;
    return veml7700_update_config();
}

esp_err_t veml7700_set_integration_time(veml7700_integration_time_t it)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_config.integration_time = it;
    return veml7700_update_config();
}

bool veml7700_is_connected(void)
{
    uint16_t dummy;
    return (veml7700_read_reg(VEML7700_REG_ALS_CONF, &dummy) == ESP_OK);
}
