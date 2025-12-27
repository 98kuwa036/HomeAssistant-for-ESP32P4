/**
 * @file ens160.c
 * @brief ScioSense ENS160 Air Quality Sensor Driver Implementation
 */

#include "ens160.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ENS160";

// Register addresses
#define ENS160_REG_PART_ID          0x00
#define ENS160_REG_OPMODE           0x10
#define ENS160_REG_CONFIG           0x11
#define ENS160_REG_COMMAND          0x12
#define ENS160_REG_TEMP_IN          0x13
#define ENS160_REG_RH_IN            0x15
#define ENS160_REG_DATA_STATUS      0x20
#define ENS160_REG_DATA_AQI         0x21
#define ENS160_REG_DATA_TVOC        0x22
#define ENS160_REG_DATA_ECO2        0x24

#define ENS160_PART_ID              0x0160

static i2c_port_t s_i2c_port = I2C_NUM_0;
static uint8_t s_i2c_addr = ENS160_I2C_ADDR_DEFAULT;
static bool s_initialized = false;

static esp_err_t ens160_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = { reg, value };
    return i2c_master_write_to_device(s_i2c_port, s_i2c_addr, data, 2, pdMS_TO_TICKS(100));
}

static esp_err_t ens160_read_reg(uint8_t reg, uint8_t *value)
{
    return i2c_master_write_read_device(s_i2c_port, s_i2c_addr, &reg, 1, value, 1, pdMS_TO_TICKS(100));
}

static esp_err_t ens160_read_regs(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(s_i2c_port, s_i2c_addr, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

esp_err_t ens160_init(const ens160_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    s_i2c_port = config ? config->i2c_port : I2C_NUM_0;
    s_i2c_addr = config ? config->i2c_addr : ENS160_I2C_ADDR_DEFAULT;

    if (!ens160_is_connected()) {
        ESP_LOGE(TAG, "ENS160 not found at address 0x%02X", s_i2c_addr);
        return ESP_ERR_NOT_FOUND;
    }

    // Verify Part ID
    uint8_t id_buf[2];
    esp_err_t ret = ens160_read_regs(ENS160_REG_PART_ID, id_buf, 2);
    if (ret != ESP_OK) return ret;

    uint16_t part_id = ((uint16_t)id_buf[1] << 8) | id_buf[0];
    if (part_id != ENS160_PART_ID) {
        ESP_LOGE(TAG, "Invalid Part ID: 0x%04X (expected 0x%04X)", part_id, ENS160_PART_ID);
        return ESP_ERR_NOT_FOUND;
    }

    // Set to standard operating mode
    ret = ens160_set_mode(ENS160_MODE_STANDARD);
    if (ret != ESP_OK) return ret;

    vTaskDelay(pdMS_TO_TICKS(20));

    s_initialized = true;
    ESP_LOGI(TAG, "ENS160 initialized (Part ID: 0x%04X)", part_id);
    return ESP_OK;
}

esp_err_t ens160_deinit(void)
{
    if (!s_initialized) return ESP_OK;
    ens160_set_mode(ENS160_MODE_DEEP_SLEEP);
    s_initialized = false;
    ESP_LOGI(TAG, "ENS160 deinitialized");
    return ESP_OK;
}

esp_err_t ens160_set_mode(ens160_mode_t mode)
{
    return ens160_write_reg(ENS160_REG_OPMODE, (uint8_t)mode);
}

esp_err_t ens160_read_data(ens160_data_t *data)
{
    if (!s_initialized || !data) return ESP_ERR_INVALID_ARG;

    memset(data, 0, sizeof(ens160_data_t));

    // Read status
    uint8_t status;
    esp_err_t ret = ens160_read_reg(ENS160_REG_DATA_STATUS, &status);
    if (ret != ESP_OK) return ret;

    data->status = status;
    data->data_valid = (status & 0x02) != 0; // NEWDAT bit

    if (!data->data_valid) {
        return ESP_OK; // No new data yet
    }

    // Read AQI
    uint8_t aqi;
    ret = ens160_read_reg(ENS160_REG_DATA_AQI, &aqi);
    if (ret != ESP_OK) return ret;
    data->aqi = (ens160_aqi_t)(aqi & 0x07);

    // Read TVOC (2 bytes)
    uint8_t tvoc_buf[2];
    ret = ens160_read_regs(ENS160_REG_DATA_TVOC, tvoc_buf, 2);
    if (ret != ESP_OK) return ret;
    data->tvoc = ((uint16_t)tvoc_buf[1] << 8) | tvoc_buf[0];

    // Read eCO2 (2 bytes)
    uint8_t eco2_buf[2];
    ret = ens160_read_regs(ENS160_REG_DATA_ECO2, eco2_buf, 2);
    if (ret != ESP_OK) return ret;
    data->eco2 = ((uint16_t)eco2_buf[1] << 8) | eco2_buf[0];

    data->timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    return ESP_OK;
}

esp_err_t ens160_set_compensation(float temperature, float humidity)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    // Temperature: convert to fixed point (64 = 0°C, 1 LSB = 1/64°C)
    uint16_t temp_raw = (uint16_t)((temperature + 273.15f) * 64.0f);
    uint8_t temp_data[3] = {
        ENS160_REG_TEMP_IN,
        (uint8_t)(temp_raw & 0xFF),
        (uint8_t)(temp_raw >> 8)
    };
    esp_err_t ret = i2c_master_write_to_device(s_i2c_port, s_i2c_addr, temp_data, 3, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) return ret;

    // Humidity: convert to fixed point (1 LSB = 1/512 %RH)
    uint16_t rh_raw = (uint16_t)(humidity * 512.0f);
    uint8_t rh_data[3] = {
        ENS160_REG_RH_IN,
        (uint8_t)(rh_raw & 0xFF),
        (uint8_t)(rh_raw >> 8)
    };
    return i2c_master_write_to_device(s_i2c_port, s_i2c_addr, rh_data, 3, pdMS_TO_TICKS(100));
}

bool ens160_is_connected(void)
{
    uint8_t dummy;
    return ens160_read_reg(ENS160_REG_PART_ID, &dummy) == ESP_OK;
}

const char *ens160_aqi_to_string(ens160_aqi_t aqi)
{
    switch (aqi) {
        case ENS160_AQI_EXCELLENT: return "Excellent";
        case ENS160_AQI_GOOD: return "Good";
        case ENS160_AQI_MODERATE: return "Moderate";
        case ENS160_AQI_POOR: return "Poor";
        case ENS160_AQI_UNHEALTHY: return "Unhealthy";
        default: return "Unknown";
    }
}
