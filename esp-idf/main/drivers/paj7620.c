/**
 * @file paj7620.c
 * @brief PAJ7620U2 Gesture Recognition Sensor Driver Implementation
 */

#include "paj7620.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "PAJ7620";

// Register Bank 0
#define PAJ7620_REG_BANK_SEL        0xEF
#define PAJ7620_REG_PART_ID_L       0x00
#define PAJ7620_REG_PART_ID_H       0x01
#define PAJ7620_REG_VERSION_ID      0x02
#define PAJ7620_REG_SUSPEND_CMD     0x03
#define PAJ7620_REG_GES_PS_DET      0x43
#define PAJ7620_REG_GES_RESULT_0    0x43
#define PAJ7620_REG_GES_RESULT_1    0x44
#define PAJ7620_REG_PS_RESULT_H     0x6B
#define PAJ7620_REG_PS_RESULT_L     0x6C
#define PAJ7620_REG_OBJECT_SIZE_H   0xAC
#define PAJ7620_REG_OBJECT_SIZE_L   0xAD

// Part ID
#define PAJ7620_PART_ID             0x7620

// Initialization register arrays
static const uint8_t init_register_array[][2] = {
    {0xEF, 0x00}, // Bank 0
    {0x32, 0x29}, {0x33, 0x01}, {0x34, 0x00}, {0x35, 0x01},
    {0x36, 0x00}, {0x37, 0x07}, {0x38, 0x17}, {0x39, 0x06},
    {0x3A, 0x12}, {0x3F, 0x00}, {0x40, 0x02}, {0x41, 0xFF},
    {0x42, 0x01}, {0x46, 0x2D}, {0x47, 0x0F}, {0x48, 0x3C},
    {0x49, 0x00}, {0x4A, 0x1E}, {0x4B, 0x00}, {0x4C, 0x20},
    {0x4D, 0x00}, {0x4E, 0x1A}, {0x4F, 0x14}, {0x50, 0x00},
    {0x51, 0x10}, {0x52, 0x00}, {0x5C, 0x02}, {0x5D, 0x00},
    {0x5E, 0x10}, {0x5F, 0x3F}, {0x60, 0x27}, {0x61, 0x28},
    {0x62, 0x00}, {0x63, 0x03}, {0x64, 0xF7}, {0x65, 0x03},
    {0x66, 0xD9}, {0x67, 0x03}, {0x68, 0x01}, {0x69, 0xC8},
    {0x6A, 0x40}, {0x6D, 0x04}, {0x6E, 0x00}, {0x6F, 0x00},
    {0x70, 0x80}, {0x71, 0x00}, {0x72, 0x00}, {0x73, 0x00},
    {0x74, 0xF0}, {0x75, 0x00}, {0x80, 0x42}, {0x81, 0x44},
    {0x82, 0x04}, {0x83, 0x20}, {0x84, 0x20}, {0x85, 0x00},
    {0x86, 0x10}, {0x87, 0x00}, {0x88, 0x05}, {0x89, 0x18},
    {0x8A, 0x10}, {0x8B, 0x01}, {0x8C, 0x37}, {0x8D, 0x00},
    {0x8E, 0xF0}, {0x8F, 0x81}, {0x90, 0x06}, {0x91, 0x06},
    {0x92, 0x1E}, {0x93, 0x0D}, {0x94, 0x0A}, {0x95, 0x0A},
    {0x96, 0x0C}, {0x97, 0x05}, {0x98, 0x0A}, {0x99, 0x41},
    {0x9A, 0x14}, {0x9B, 0x0A}, {0x9C, 0x3F}, {0x9D, 0x33},
    {0x9E, 0xAE}, {0x9F, 0xF9}, {0xA0, 0x48}, {0xA1, 0x13},
    {0xA2, 0x10}, {0xA3, 0x08}, {0xA4, 0x30}, {0xA5, 0x19},
    {0xA6, 0x10}, {0xA7, 0x08}, {0xA8, 0x24}, {0xA9, 0x04},
    {0xAA, 0x1E}, {0xAB, 0x1E}, {0xCC, 0x19}, {0xCD, 0x0B},
    {0xCE, 0x13}, {0xCF, 0x64}, {0xD0, 0x21},
    {0xEF, 0x01}, // Bank 1
    {0x02, 0x0F}, {0x03, 0x10}, {0x04, 0x02}, {0x25, 0x01},
    {0x27, 0x39}, {0x28, 0x7F}, {0x29, 0x08}, {0x3E, 0xFF},
    {0x5E, 0x3D}, {0x65, 0x96}, {0x67, 0x97}, {0x69, 0xCD},
    {0x6A, 0x01}, {0x6D, 0x2C}, {0x6E, 0x01}, {0x72, 0x01},
    {0x73, 0x35}, {0x77, 0x01},
    {0xEF, 0x00}, // Back to Bank 0
    {0x41, 0xFF}, {0x42, 0x01},
};

static i2c_port_t s_i2c_port = I2C_NUM_0;
static bool s_initialized = false;
static paj7620_mode_t s_current_mode = PAJ7620_MODE_GESTURE;

/**
 * @brief Select register bank
 */
static esp_err_t paj7620_select_bank(uint8_t bank)
{
    uint8_t data[2] = {PAJ7620_REG_BANK_SEL, bank};
    return i2c_master_write_to_device(s_i2c_port, PAJ7620_I2C_ADDR,
                                       data, 2, pdMS_TO_TICKS(100));
}

/**
 * @brief Write single byte
 */
static esp_err_t paj7620_write_byte(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    return i2c_master_write_to_device(s_i2c_port, PAJ7620_I2C_ADDR,
                                       buf, 2, pdMS_TO_TICKS(100));
}

/**
 * @brief Read single byte
 */
static esp_err_t paj7620_read_byte(uint8_t reg, uint8_t *data)
{
    return i2c_master_write_read_device(s_i2c_port, PAJ7620_I2C_ADDR,
                                         &reg, 1, data, 1, pdMS_TO_TICKS(100));
}

/**
 * @brief Read two bytes
 */
static esp_err_t paj7620_read_word(uint8_t reg, uint16_t *data)
{
    uint8_t buf[2];
    esp_err_t ret = i2c_master_write_read_device(s_i2c_port, PAJ7620_I2C_ADDR,
                                                  &reg, 1, buf, 2, pdMS_TO_TICKS(100));
    if (ret == ESP_OK) {
        *data = ((uint16_t)buf[1] << 8) | buf[0];
    }
    return ret;
}

esp_err_t paj7620_init(const paj7620_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    s_i2c_port = config->i2c_port;

    // Wake up device
    paj7620_select_bank(0);
    vTaskDelay(pdMS_TO_TICKS(10));
    paj7620_select_bank(0);

    // Verify part ID
    uint16_t part_id;
    esp_err_t ret = paj7620_read_word(PAJ7620_REG_PART_ID_L, &part_id);
    if (ret != ESP_OK || part_id != PAJ7620_PART_ID) {
        ESP_LOGE(TAG, "PAJ7620 not found (ID: 0x%04X, expected 0x%04X)", part_id, PAJ7620_PART_ID);
        return ESP_ERR_NOT_FOUND;
    }

    // Load initialization sequence
    for (size_t i = 0; i < sizeof(init_register_array) / 2; i++) {
        ret = paj7620_write_byte(init_register_array[i][0], init_register_array[i][1]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Init sequence failed at index %zu", i);
            return ret;
        }
    }

    // Set requested mode
    ret = paj7620_set_mode(config->mode);
    if (ret != ESP_OK) {
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "PAJ7620U2 initialized in %s mode",
             config->mode == PAJ7620_MODE_GESTURE ? "Gesture" : "Proximity");

    return ESP_OK;
}

esp_err_t paj7620_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    // Put device in suspend mode
    paj7620_select_bank(0);
    paj7620_write_byte(PAJ7620_REG_SUSPEND_CMD, 0x01);

    s_initialized = false;
    ESP_LOGI(TAG, "PAJ7620U2 deinitialized");
    return ESP_OK;
}

esp_err_t paj7620_set_mode(paj7620_mode_t mode)
{
    if (!s_initialized && mode != PAJ7620_MODE_GESTURE) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = paj7620_select_bank(0);
    if (ret != ESP_OK) {
        return ret;
    }

    if (mode == PAJ7620_MODE_GESTURE) {
        // Gesture mode configuration
        paj7620_write_byte(0x41, 0xFF);
        paj7620_write_byte(0x42, 0x01);
    } else {
        // Proximity mode configuration
        paj7620_write_byte(0x41, 0x00);
        paj7620_write_byte(0x42, 0x00);
    }

    s_current_mode = mode;
    return ESP_OK;
}

bool paj7620_is_gesture_available(void)
{
    if (!s_initialized || s_current_mode != PAJ7620_MODE_GESTURE) {
        return false;
    }

    uint8_t ges_result;
    paj7620_select_bank(0);

    if (paj7620_read_byte(PAJ7620_REG_GES_RESULT_0, &ges_result) == ESP_OK) {
        return (ges_result != 0);
    }

    return false;
}

esp_err_t paj7620_read_gesture(paj7620_data_t *data)
{
    if (!s_initialized || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(data, 0, sizeof(paj7620_data_t));

    paj7620_select_bank(0);

    // Read gesture result registers
    uint8_t ges_result_0, ges_result_1;
    esp_err_t ret = paj7620_read_byte(PAJ7620_REG_GES_RESULT_0, &ges_result_0);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = paj7620_read_byte(PAJ7620_REG_GES_RESULT_1, &ges_result_1);
    if (ret != ESP_OK) {
        return ret;
    }

    // Parse gesture
    if (ges_result_0 != 0 || ges_result_1 != 0) {
        data->gesture_detected = true;
        data->timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

        if (ges_result_0 & 0x01) data->gesture = PAJ7620_GESTURE_UP;
        else if (ges_result_0 & 0x02) data->gesture = PAJ7620_GESTURE_DOWN;
        else if (ges_result_0 & 0x04) data->gesture = PAJ7620_GESTURE_LEFT;
        else if (ges_result_0 & 0x08) data->gesture = PAJ7620_GESTURE_RIGHT;
        else if (ges_result_0 & 0x10) data->gesture = PAJ7620_GESTURE_FORWARD;
        else if (ges_result_0 & 0x20) data->gesture = PAJ7620_GESTURE_BACKWARD;
        else if (ges_result_0 & 0x40) data->gesture = PAJ7620_GESTURE_CLOCKWISE;
        else if (ges_result_0 & 0x80) data->gesture = PAJ7620_GESTURE_COUNTERCLOCKWISE;
        else if (ges_result_1 & 0x01) {
            data->gesture = PAJ7620_GESTURE_WAVE;
            data->wave_count = (ges_result_1 >> 4) & 0x0F;
        }
    }

    return ESP_OK;
}

esp_err_t paj7620_read_proximity(paj7620_proximity_t *data)
{
    if (!s_initialized || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(data, 0, sizeof(paj7620_proximity_t));

    paj7620_select_bank(0);

    // Read proximity result
    uint8_t ps_h, ps_l;
    paj7620_read_byte(PAJ7620_REG_PS_RESULT_H, &ps_h);
    paj7620_read_byte(PAJ7620_REG_PS_RESULT_L, &ps_l);

    data->brightness = ps_l;
    data->object_detected = (ps_h & 0x80) != 0;

    // Read object size
    uint8_t size_h, size_l;
    paj7620_read_byte(PAJ7620_REG_OBJECT_SIZE_H, &size_h);
    paj7620_read_byte(PAJ7620_REG_OBJECT_SIZE_L, &size_l);
    data->object_size = ((uint16_t)size_h << 8) | size_l;

    return ESP_OK;
}

bool paj7620_is_connected(void)
{
    uint16_t part_id;
    paj7620_select_bank(0);
    vTaskDelay(pdMS_TO_TICKS(5));

    if (paj7620_read_word(PAJ7620_REG_PART_ID_L, &part_id) == ESP_OK) {
        return (part_id == PAJ7620_PART_ID);
    }
    return false;
}

const char *paj7620_gesture_to_string(paj7620_gesture_t gesture)
{
    switch (gesture) {
        case PAJ7620_GESTURE_UP: return "Up";
        case PAJ7620_GESTURE_DOWN: return "Down";
        case PAJ7620_GESTURE_LEFT: return "Left";
        case PAJ7620_GESTURE_RIGHT: return "Right";
        case PAJ7620_GESTURE_FORWARD: return "Forward";
        case PAJ7620_GESTURE_BACKWARD: return "Backward";
        case PAJ7620_GESTURE_CLOCKWISE: return "Clockwise";
        case PAJ7620_GESTURE_COUNTERCLOCKWISE: return "Counter-clockwise";
        case PAJ7620_GESTURE_WAVE: return "Wave";
        default: return "None";
    }
}
