/**
 * @file vl53l1x.c
 * @brief VL53L1X Time-of-Flight Distance Sensor Driver Implementation
 *
 * Based on ST's Ultra Lite Driver (ULD) API
 */

#include "vl53l1x.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "VL53L1X";

// Register addresses (subset for basic operation)
#define VL53L1X_REG_SOFT_RESET                      0x0000
#define VL53L1X_REG_I2C_SLAVE_DEVICE_ADDRESS        0x0001
#define VL53L1X_REG_MODEL_ID                        0x010F
#define VL53L1X_REG_MODULE_TYPE                     0x0110
#define VL53L1X_REG_SYSTEM_STATUS                   0x00E5
#define VL53L1X_REG_SYSTEM_MODE_START               0x0087
#define VL53L1X_REG_GPIO_HV_MUX_CTRL                0x0030
#define VL53L1X_REG_GPIO_TIO_HV_STATUS              0x0031
#define VL53L1X_REG_SYSTEM_INTERRUPT_CLEAR          0x0086
#define VL53L1X_REG_RESULT_RANGE_STATUS             0x0089
#define VL53L1X_REG_RESULT_FINAL_RANGE_MM           0x0096
#define VL53L1X_REG_RESULT_PEAK_SIGNAL_RATE         0x0098
#define VL53L1X_REG_RESULT_AMBIENT_RATE             0x009A
#define VL53L1X_REG_RANGE_CONFIG_TIMEOUT_MACROP_A   0x005E
#define VL53L1X_REG_RANGE_CONFIG_VCSEL_PERIOD_A     0x0060
#define VL53L1X_REG_SD_CONFIG_WOI_SD0               0x0078
#define VL53L1X_REG_INTERMEASUREMENT_MS             0x006C

// Expected model ID
#define VL53L1X_MODEL_ID                            0xEA

static i2c_port_t s_i2c_port = I2C_NUM_0;
static bool s_initialized = false;
static vl53l1x_config_t s_config;

/**
 * @brief Write single byte to register
 */
static esp_err_t vl53l1x_write_byte(uint16_t reg, uint8_t data)
{
    uint8_t buf[3] = {
        (uint8_t)(reg >> 8),
        (uint8_t)(reg & 0xFF),
        data
    };

    return i2c_master_write_to_device(s_i2c_port, VL53L1X_I2C_ADDR,
                                       buf, 3, pdMS_TO_TICKS(100));
}

/**
 * @brief Write two bytes to register
 */
static esp_err_t vl53l1x_write_word(uint16_t reg, uint16_t data)
{
    uint8_t buf[4] = {
        (uint8_t)(reg >> 8),
        (uint8_t)(reg & 0xFF),
        (uint8_t)(data >> 8),
        (uint8_t)(data & 0xFF)
    };

    return i2c_master_write_to_device(s_i2c_port, VL53L1X_I2C_ADDR,
                                       buf, 4, pdMS_TO_TICKS(100));
}

/**
 * @brief Write four bytes to register
 */
static esp_err_t vl53l1x_write_dword(uint16_t reg, uint32_t data)
{
    uint8_t buf[6] = {
        (uint8_t)(reg >> 8),
        (uint8_t)(reg & 0xFF),
        (uint8_t)(data >> 24),
        (uint8_t)(data >> 16),
        (uint8_t)(data >> 8),
        (uint8_t)(data & 0xFF)
    };

    return i2c_master_write_to_device(s_i2c_port, VL53L1X_I2C_ADDR,
                                       buf, 6, pdMS_TO_TICKS(100));
}

/**
 * @brief Read single byte from register
 */
static esp_err_t vl53l1x_read_byte(uint16_t reg, uint8_t *data)
{
    uint8_t addr[2] = {
        (uint8_t)(reg >> 8),
        (uint8_t)(reg & 0xFF)
    };

    return i2c_master_write_read_device(s_i2c_port, VL53L1X_I2C_ADDR,
                                         addr, 2, data, 1, pdMS_TO_TICKS(100));
}

/**
 * @brief Read two bytes from register
 */
static esp_err_t vl53l1x_read_word(uint16_t reg, uint16_t *data)
{
    uint8_t addr[2] = {
        (uint8_t)(reg >> 8),
        (uint8_t)(reg & 0xFF)
    };
    uint8_t buf[2];

    esp_err_t ret = i2c_master_write_read_device(s_i2c_port, VL53L1X_I2C_ADDR,
                                                  addr, 2, buf, 2, pdMS_TO_TICKS(100));
    if (ret == ESP_OK) {
        *data = ((uint16_t)buf[0] << 8) | buf[1];
    }
    return ret;
}

/**
 * @brief Sensor boot sequence (simplified from ULD)
 */
static esp_err_t vl53l1x_sensor_init(void)
{
    // Wait for boot
    uint8_t status = 0;
    int timeout = 100;

    while (timeout-- > 0) {
        vl53l1x_read_byte(VL53L1X_REG_SYSTEM_STATUS, &status);
        if (status & 0x01) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (timeout <= 0) {
        ESP_LOGE(TAG, "Sensor boot timeout");
        return ESP_ERR_TIMEOUT;
    }

    // Check model ID
    uint8_t model_id;
    esp_err_t ret = vl53l1x_read_byte(VL53L1X_REG_MODEL_ID, &model_id);
    if (ret != ESP_OK || model_id != VL53L1X_MODEL_ID) {
        ESP_LOGE(TAG, "Invalid model ID: 0x%02X (expected 0x%02X)", model_id, VL53L1X_MODEL_ID);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "VL53L1X detected (Model ID: 0x%02X)", model_id);
    return ESP_OK;
}

esp_err_t vl53l1x_init(const vl53l1x_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    s_i2c_port = config->i2c_port;
    memcpy(&s_config, config, sizeof(vl53l1x_config_t));

    // Check connection
    if (!vl53l1x_is_connected()) {
        ESP_LOGE(TAG, "VL53L1X not found at address 0x%02X", VL53L1X_I2C_ADDR);
        return ESP_ERR_NOT_FOUND;
    }

    // Initialize sensor
    esp_err_t ret = vl53l1x_sensor_init();
    if (ret != ESP_OK) {
        return ret;
    }

    // Configure distance mode
    ret = vl53l1x_set_distance_mode(config->distance_mode);
    if (ret != ESP_OK) {
        return ret;
    }

    // Configure timing budget
    ret = vl53l1x_set_timing_budget(config->timing_budget_ms);
    if (ret != ESP_OK) {
        return ret;
    }

    // Configure inter-measurement period
    if (config->inter_measurement_ms > 0) {
        ret = vl53l1x_set_inter_measurement(config->inter_measurement_ms);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    s_initialized = true;
    ESP_LOGI(TAG, "VL53L1X initialized (Mode: %s, Timing: %dms)",
             config->distance_mode == VL53L1X_DISTANCE_MODE_SHORT ? "Short" : "Long",
             config->timing_budget_ms);

    return ESP_OK;
}

esp_err_t vl53l1x_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    vl53l1x_stop_ranging();
    s_initialized = false;

    ESP_LOGI(TAG, "VL53L1X deinitialized");
    return ESP_OK;
}

esp_err_t vl53l1x_start_ranging(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Clear interrupt
    vl53l1x_write_byte(VL53L1X_REG_SYSTEM_INTERRUPT_CLEAR, 0x01);

    // Start continuous ranging
    return vl53l1x_write_byte(VL53L1X_REG_SYSTEM_MODE_START, 0x40);
}

esp_err_t vl53l1x_stop_ranging(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    return vl53l1x_write_byte(VL53L1X_REG_SYSTEM_MODE_START, 0x00);
}

bool vl53l1x_is_data_ready(void)
{
    if (!s_initialized) {
        return false;
    }

    uint8_t gpio_status;
    esp_err_t ret = vl53l1x_read_byte(VL53L1X_REG_GPIO_TIO_HV_STATUS, &gpio_status);

    if (ret != ESP_OK) {
        return false;
    }

    // Check interrupt polarity and status
    uint8_t polarity;
    vl53l1x_read_byte(VL53L1X_REG_GPIO_HV_MUX_CTRL, &polarity);
    polarity = (polarity & 0x10) >> 4;

    return ((gpio_status & 0x01) == polarity);
}

esp_err_t vl53l1x_read_data(vl53l1x_data_t *data)
{
    if (!s_initialized || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(data, 0, sizeof(vl53l1x_data_t));

    // Read range status
    uint8_t status;
    esp_err_t ret = vl53l1x_read_byte(VL53L1X_REG_RESULT_RANGE_STATUS, &status);
    if (ret != ESP_OK) {
        return ret;
    }
    data->range_status = (status & 0x1F);

    // Read distance
    ret = vl53l1x_read_word(VL53L1X_REG_RESULT_FINAL_RANGE_MM, &data->distance_mm);
    if (ret != ESP_OK) {
        return ret;
    }

    // Read signal rate
    vl53l1x_read_word(VL53L1X_REG_RESULT_PEAK_SIGNAL_RATE, &data->signal_rate);

    // Read ambient rate
    vl53l1x_read_word(VL53L1X_REG_RESULT_AMBIENT_RATE, &data->ambient_rate);

    // Clear interrupt for next measurement
    vl53l1x_write_byte(VL53L1X_REG_SYSTEM_INTERRUPT_CLEAR, 0x01);

    // Set validity
    data->valid = (data->range_status == VL53L1X_RANGE_VALID) &&
                  (data->distance_mm > 0) &&
                  (data->distance_mm < 4000);
    data->timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    return ESP_OK;
}

esp_err_t vl53l1x_set_distance_mode(vl53l1x_distance_mode_t mode)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Distance mode affects VCSEL period and signal thresholds
    if (mode == VL53L1X_DISTANCE_MODE_SHORT) {
        vl53l1x_write_byte(VL53L1X_REG_RANGE_CONFIG_VCSEL_PERIOD_A, 0x07);
        vl53l1x_write_byte(VL53L1X_REG_SD_CONFIG_WOI_SD0, 0x07);
    } else {
        vl53l1x_write_byte(VL53L1X_REG_RANGE_CONFIG_VCSEL_PERIOD_A, 0x0F);
        vl53l1x_write_byte(VL53L1X_REG_SD_CONFIG_WOI_SD0, 0x0F);
    }

    s_config.distance_mode = mode;
    return ESP_OK;
}

esp_err_t vl53l1x_set_timing_budget(vl53l1x_timing_budget_t budget_ms)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Timing budget lookup table (simplified)
    uint16_t timeout_a;
    switch (budget_ms) {
        case VL53L1X_TIMING_15MS:  timeout_a = 0x001D; break;
        case VL53L1X_TIMING_20MS:  timeout_a = 0x0026; break;
        case VL53L1X_TIMING_33MS:  timeout_a = 0x003C; break;
        case VL53L1X_TIMING_50MS:  timeout_a = 0x0057; break;
        case VL53L1X_TIMING_100MS: timeout_a = 0x00A8; break;
        case VL53L1X_TIMING_200MS: timeout_a = 0x0148; break;
        case VL53L1X_TIMING_500MS: timeout_a = 0x0320; break;
        default: return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = vl53l1x_write_word(VL53L1X_REG_RANGE_CONFIG_TIMEOUT_MACROP_A, timeout_a);
    if (ret == ESP_OK) {
        s_config.timing_budget_ms = budget_ms;
    }
    return ret;
}

esp_err_t vl53l1x_set_inter_measurement(uint16_t period_ms)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Inter-measurement period in clock ticks (1.065ms per tick)
    uint32_t ticks = (uint32_t)(period_ms * 1.065f);

    esp_err_t ret = vl53l1x_write_dword(VL53L1X_REG_INTERMEASUREMENT_MS, ticks);
    if (ret == ESP_OK) {
        s_config.inter_measurement_ms = period_ms;
    }
    return ret;
}

bool vl53l1x_is_connected(void)
{
    uint8_t model_id;
    esp_err_t ret = vl53l1x_read_byte(VL53L1X_REG_MODEL_ID, &model_id);
    return (ret == ESP_OK && model_id == VL53L1X_MODEL_ID);
}
