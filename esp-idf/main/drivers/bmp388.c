/**
 * @file bmp388.c
 * @brief Bosch BMP388 Barometric Pressure Sensor Driver Implementation
 */

#include "bmp388.h"
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BMP388";

// Register addresses
#define BMP388_REG_CHIP_ID          0x00
#define BMP388_REG_ERR_REG          0x02
#define BMP388_REG_STATUS           0x03
#define BMP388_REG_DATA_0           0x04
#define BMP388_REG_DATA_1           0x05
#define BMP388_REG_DATA_2           0x06
#define BMP388_REG_DATA_3           0x07
#define BMP388_REG_DATA_4           0x08
#define BMP388_REG_DATA_5           0x09
#define BMP388_REG_INT_STATUS       0x11
#define BMP388_REG_PWR_CTRL         0x1B
#define BMP388_REG_OSR              0x1C
#define BMP388_REG_ODR              0x1D
#define BMP388_REG_CONFIG           0x1F
#define BMP388_REG_CALIB_DATA       0x31
#define BMP388_REG_CMD              0x7E

#define BMP388_CHIP_ID              0x50

// Calibration data structure
typedef struct {
    uint16_t par_t1;
    uint16_t par_t2;
    int8_t par_t3;
    int16_t par_p1;
    int16_t par_p2;
    int8_t par_p3;
    int8_t par_p4;
    uint16_t par_p5;
    uint16_t par_p6;
    int8_t par_p7;
    int8_t par_p8;
    int16_t par_p9;
    int8_t par_p10;
    int8_t par_p11;
    float t_lin;
} bmp388_calib_t;

static i2c_port_t s_i2c_port = I2C_NUM_0;
static uint8_t s_i2c_addr = BMP388_I2C_ADDR_DEFAULT;
static bool s_initialized = false;
static bmp388_calib_t s_calib;
static float s_sea_level_pressure = 1013.25f;

static esp_err_t bmp388_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = { reg, value };
    return i2c_master_write_to_device(s_i2c_port, s_i2c_addr, data, 2, pdMS_TO_TICKS(100));
}

static esp_err_t bmp388_read_regs(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(s_i2c_port, s_i2c_addr, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

static esp_err_t bmp388_read_calibration(void)
{
    uint8_t calib[21];
    esp_err_t ret = bmp388_read_regs(BMP388_REG_CALIB_DATA, calib, 21);
    if (ret != ESP_OK) return ret;

    s_calib.par_t1 = ((uint16_t)calib[1] << 8) | calib[0];
    s_calib.par_t2 = ((uint16_t)calib[3] << 8) | calib[2];
    s_calib.par_t3 = (int8_t)calib[4];
    s_calib.par_p1 = ((int16_t)calib[6] << 8) | calib[5];
    s_calib.par_p2 = ((int16_t)calib[8] << 8) | calib[7];
    s_calib.par_p3 = (int8_t)calib[9];
    s_calib.par_p4 = (int8_t)calib[10];
    s_calib.par_p5 = ((uint16_t)calib[12] << 8) | calib[11];
    s_calib.par_p6 = ((uint16_t)calib[14] << 8) | calib[13];
    s_calib.par_p7 = (int8_t)calib[15];
    s_calib.par_p8 = (int8_t)calib[16];
    s_calib.par_p9 = ((int16_t)calib[18] << 8) | calib[17];
    s_calib.par_p10 = (int8_t)calib[19];
    s_calib.par_p11 = (int8_t)calib[20];

    return ESP_OK;
}

static float bmp388_compensate_temperature(uint32_t raw_temp)
{
    float partial_data1 = (float)(raw_temp - (256.0f * s_calib.par_t1));
    float partial_data2 = s_calib.par_t2 * (1.0f / 1073741824.0f);
    s_calib.t_lin = partial_data1 * partial_data2;

    partial_data2 = partial_data1 * (1.0f / 1073741824.0f);
    float partial_data3 = partial_data2 * partial_data1;
    float partial_data4 = s_calib.par_t3 * (1.0f / 281474976710656.0f);
    s_calib.t_lin += partial_data3 * partial_data4;

    return s_calib.t_lin;
}

static float bmp388_compensate_pressure(uint32_t raw_press)
{
    float partial_data1, partial_data2, partial_data3, partial_data4;
    float partial_out1, partial_out2;

    partial_data1 = s_calib.par_p6 * s_calib.t_lin;
    partial_data2 = s_calib.par_p7 * (s_calib.t_lin * s_calib.t_lin);
    partial_data3 = s_calib.par_p8 * (s_calib.t_lin * s_calib.t_lin * s_calib.t_lin);
    partial_out1 = s_calib.par_p5 + partial_data1 + partial_data2 + partial_data3;

    partial_data1 = s_calib.par_p2 * s_calib.t_lin;
    partial_data2 = s_calib.par_p3 * (s_calib.t_lin * s_calib.t_lin);
    partial_data3 = s_calib.par_p4 * (s_calib.t_lin * s_calib.t_lin * s_calib.t_lin);
    partial_out2 = (float)raw_press * (s_calib.par_p1 + partial_data1 + partial_data2 + partial_data3);

    partial_data1 = (float)raw_press * (float)raw_press;
    partial_data2 = s_calib.par_p9 + s_calib.par_p10 * s_calib.t_lin;
    partial_data3 = partial_data1 * partial_data2;
    partial_data4 = partial_data3 + ((float)raw_press * (float)raw_press * (float)raw_press) * s_calib.par_p11;

    return partial_out1 + partial_out2 + partial_data4;
}

esp_err_t bmp388_init(const bmp388_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    if (config) {
        s_i2c_port = config->i2c_port;
        s_i2c_addr = config->i2c_addr;
        s_sea_level_pressure = config->sea_level_pressure;
    }

    if (!bmp388_is_connected()) {
        ESP_LOGE(TAG, "BMP388 not found at address 0x%02X", s_i2c_addr);
        return ESP_ERR_NOT_FOUND;
    }

    // Read and verify chip ID
    uint8_t chip_id;
    esp_err_t ret = bmp388_read_regs(BMP388_REG_CHIP_ID, &chip_id, 1);
    if (ret != ESP_OK || chip_id != BMP388_CHIP_ID) {
        ESP_LOGE(TAG, "Invalid Chip ID: 0x%02X (expected 0x%02X)", chip_id, BMP388_CHIP_ID);
        return ESP_ERR_NOT_FOUND;
    }

    // Soft reset
    bmp388_write_reg(BMP388_REG_CMD, 0xB6);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Read calibration data
    ret = bmp388_read_calibration();
    if (ret != ESP_OK) return ret;

    // Configure OSR
    uint8_t osr = ((config ? config->temperature_osr : BMP388_OSR_X1) << 3) |
                  (config ? config->pressure_osr : BMP388_OSR_X8);
    bmp388_write_reg(BMP388_REG_OSR, osr);

    // Configure ODR
    bmp388_write_reg(BMP388_REG_ODR, config ? config->odr : BMP388_ODR_50HZ);

    // Enable pressure and temperature measurement, normal mode
    bmp388_write_reg(BMP388_REG_PWR_CTRL, 0x33);

    s_initialized = true;
    ESP_LOGI(TAG, "BMP388 initialized");
    return ESP_OK;
}

esp_err_t bmp388_deinit(void)
{
    if (!s_initialized) return ESP_OK;
    bmp388_write_reg(BMP388_REG_PWR_CTRL, 0x00); // Sleep mode
    s_initialized = false;
    ESP_LOGI(TAG, "BMP388 deinitialized");
    return ESP_OK;
}

esp_err_t bmp388_read_data(bmp388_data_t *data)
{
    if (!s_initialized || !data) return ESP_ERR_INVALID_ARG;

    memset(data, 0, sizeof(bmp388_data_t));

    // Read raw data (6 bytes: pressure[3] + temperature[3])
    uint8_t raw_data[6];
    esp_err_t ret = bmp388_read_regs(BMP388_REG_DATA_0, raw_data, 6);
    if (ret != ESP_OK) return ret;

    uint32_t raw_press = ((uint32_t)raw_data[2] << 16) | ((uint32_t)raw_data[1] << 8) | raw_data[0];
    uint32_t raw_temp = ((uint32_t)raw_data[5] << 16) | ((uint32_t)raw_data[4] << 8) | raw_data[3];

    // Compensate readings
    data->temperature = bmp388_compensate_temperature(raw_temp);
    data->pressure = bmp388_compensate_pressure(raw_press) / 100.0f; // Convert to hPa

    // Calculate altitude using barometric formula
    data->altitude = 44330.0f * (1.0f - powf(data->pressure / s_sea_level_pressure, 0.1903f));

    data->timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    return ESP_OK;
}

esp_err_t bmp388_set_sea_level_pressure(float pressure)
{
    s_sea_level_pressure = pressure;
    return ESP_OK;
}

bool bmp388_is_connected(void)
{
    uint8_t chip_id;
    return bmp388_read_regs(BMP388_REG_CHIP_ID, &chip_id, 1) == ESP_OK;
}
