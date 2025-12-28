/**
 * @file sensor_hub.c
 * @brief Multi-Sensor Hub Implementation
 */

#include "sensor_hub.h"
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/i2c_master.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static const char *TAG = "sensor_hub";

// ============================================================================
// I2C Sensor Addresses (from Kconfig)
// ============================================================================

#define SHT40_ADDR      CONFIG_SHT40_I2C_ADDR
#define BMP388_ADDR     CONFIG_BMP388_I2C_ADDR
#define SCD41_ADDR      CONFIG_SCD41_I2C_ADDR
#define ENS160_ADDR     CONFIG_ENS160_I2C_ADDR
#define SGP40_ADDR      CONFIG_SGP40_I2C_ADDR
#define BH1750_ADDR     CONFIG_BH1750_I2C_ADDR

// ============================================================================
// Sensor Command Constants
// ============================================================================

// SHT40 commands
#define SHT40_CMD_MEASURE_HIGH      0xFD

// BMP388 registers
#define BMP388_REG_CHIP_ID          0x00
#define BMP388_REG_DATA             0x04
#define BMP388_REG_PWR_CTRL         0x1B
#define BMP388_REG_OSR              0x1C
#define BMP388_REG_ODR              0x1D
#define BMP388_CHIP_ID              0x50

// SCD41 commands
#define SCD41_CMD_START_MEASUREMENT 0x21B1
#define SCD41_CMD_READ_MEASUREMENT  0xEC05
#define SCD41_CMD_STOP_MEASUREMENT  0x3F86

// ENS160 registers
#define ENS160_REG_PART_ID          0x00
#define ENS160_REG_OPMODE           0x10
#define ENS160_REG_DATA_AQI         0x21
#define ENS160_REG_DATA_TVOC        0x22
#define ENS160_REG_DATA_ECO2        0x24
#define ENS160_PART_ID              0x0160

// SGP40 commands
#define SGP40_CMD_MEASURE_RAW       0x260F

// BH1750 commands
#define BH1750_CMD_POWER_ON         0x01
#define BH1750_CMD_CONTINUOUS_HIGH  0x10

// ============================================================================
// Internal State
// ============================================================================

typedef struct {
    i2c_master_bus_handle_t i2c_bus;
    i2c_master_dev_handle_t dev_sht40;
    i2c_master_dev_handle_t dev_bmp388;
    i2c_master_dev_handle_t dev_scd41;
    i2c_master_dev_handle_t dev_ens160;
    i2c_master_dev_handle_t dev_sgp40;
    i2c_master_dev_handle_t dev_bh1750;

    QueueHandle_t ld2410_queue;
    QueueHandle_t sen0540_queue;

    sensor_status_t status;
    SemaphoreHandle_t mutex;
    bool initialized;
} sensor_hub_state_t;

static sensor_hub_state_t s_hub = {0};

// ============================================================================
// I2C Helper Functions
// ============================================================================

static esp_err_t i2c_write_cmd(i2c_master_dev_handle_t dev, uint16_t cmd)
{
    uint8_t data[2] = {cmd >> 8, cmd & 0xFF};
    return i2c_master_transmit(dev, data, 2, 100);
}

static esp_err_t i2c_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    return i2c_master_transmit(dev, data, 2, 100);
}

static esp_err_t i2c_read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(dev, &reg, 1, buf, len, 100);
}

// ============================================================================
// Individual Sensor Drivers
// ============================================================================

/**
 * @brief Read SHT40 temperature and humidity
 */
static esp_err_t read_sht40(sensor_data_t *data)
{
    if (!s_hub.dev_sht40) return ESP_ERR_INVALID_STATE;

    uint8_t cmd = SHT40_CMD_MEASURE_HIGH;
    uint8_t buf[6];

    esp_err_t ret = i2c_master_transmit(s_hub.dev_sht40, &cmd, 1, 100);
    if (ret != ESP_OK) return ret;

    vTaskDelay(pdMS_TO_TICKS(10));  // Measurement time

    ret = i2c_master_receive(s_hub.dev_sht40, buf, 6, 100);
    if (ret != ESP_OK) return ret;

    // Parse temperature (skip CRC at buf[2])
    uint16_t t_raw = (buf[0] << 8) | buf[1];
    data->temperature = -45.0f + 175.0f * ((float)t_raw / 65535.0f);

    // Parse humidity (skip CRC at buf[5])
    uint16_t h_raw = (buf[3] << 8) | buf[4];
    data->humidity = -6.0f + 125.0f * ((float)h_raw / 65535.0f);

    // Clamp humidity
    if (data->humidity < 0) data->humidity = 0;
    if (data->humidity > 100) data->humidity = 100;

    data->sht40_valid = true;
    return ESP_OK;
}

/**
 * @brief Read BMP388 pressure
 */
static esp_err_t read_bmp388(sensor_data_t *data)
{
    if (!s_hub.dev_bmp388) return ESP_ERR_INVALID_STATE;

    uint8_t buf[6];
    esp_err_t ret = i2c_read_reg(s_hub.dev_bmp388, BMP388_REG_DATA, buf, 6);
    if (ret != ESP_OK) return ret;

    // Parse pressure (24-bit)
    uint32_t p_raw = buf[0] | (buf[1] << 8) | (buf[2] << 16);

    // Parse temperature (24-bit)
    uint32_t t_raw = buf[3] | (buf[4] << 8) | (buf[5] << 16);

    // Simplified calculation (should use calibration data)
    data->pressure = (float)p_raw / 256.0f / 100.0f;  // hPa

    // Calculate altitude from pressure (ISA formula)
    data->altitude = 44330.0f * (1.0f - powf(data->pressure / 1013.25f, 0.1903f));

    data->bmp388_valid = true;
    return ESP_OK;
}

/**
 * @brief Read SCD41 CO2
 */
static esp_err_t read_scd41(sensor_data_t *data)
{
    if (!s_hub.dev_scd41) return ESP_ERR_INVALID_STATE;

    // Send read measurement command
    esp_err_t ret = i2c_write_cmd(s_hub.dev_scd41, SCD41_CMD_READ_MEASUREMENT);
    if (ret != ESP_OK) return ret;

    vTaskDelay(pdMS_TO_TICKS(1));

    uint8_t buf[9];
    ret = i2c_master_receive(s_hub.dev_scd41, buf, 9, 100);
    if (ret != ESP_OK) return ret;

    // Parse CO2 (skip CRC bytes)
    data->co2 = (buf[0] << 8) | buf[1];

    // Parse temperature
    uint16_t t_raw = (buf[3] << 8) | buf[4];
    data->scd41_temperature = -45.0f + 175.0f * ((float)t_raw / 65535.0f);

    // Parse humidity
    uint16_t h_raw = (buf[6] << 8) | buf[7];
    data->scd41_humidity = 100.0f * ((float)h_raw / 65535.0f);

    data->scd41_valid = true;
    return ESP_OK;
}

/**
 * @brief Read ENS160 air quality
 */
static esp_err_t read_ens160(sensor_data_t *data)
{
    if (!s_hub.dev_ens160) return ESP_ERR_INVALID_STATE;

    uint8_t buf[5];

    // Read AQI
    esp_err_t ret = i2c_read_reg(s_hub.dev_ens160, ENS160_REG_DATA_AQI, buf, 1);
    if (ret != ESP_OK) return ret;
    data->aqi = (air_quality_level_t)(buf[0] & 0x07);

    // Read TVOC
    ret = i2c_read_reg(s_hub.dev_ens160, ENS160_REG_DATA_TVOC, buf, 2);
    if (ret != ESP_OK) return ret;
    data->tvoc = buf[0] | (buf[1] << 8);

    // Read eCO2
    ret = i2c_read_reg(s_hub.dev_ens160, ENS160_REG_DATA_ECO2, buf, 2);
    if (ret != ESP_OK) return ret;
    data->eco2 = buf[0] | (buf[1] << 8);

    data->ens160_valid = true;
    return ESP_OK;
}

/**
 * @brief Read SGP40 VOC index
 */
static esp_err_t read_sgp40(sensor_data_t *data)
{
    if (!s_hub.dev_sgp40) return ESP_ERR_INVALID_STATE;

    // Send measure command with default humidity/temp compensation
    uint8_t cmd[] = {0x26, 0x0F, 0x80, 0x00, 0xA2, 0x66, 0x66, 0x93};
    esp_err_t ret = i2c_master_transmit(s_hub.dev_sgp40, cmd, sizeof(cmd), 100);
    if (ret != ESP_OK) return ret;

    vTaskDelay(pdMS_TO_TICKS(30));  // Measurement time

    uint8_t buf[3];
    ret = i2c_master_receive(s_hub.dev_sgp40, buf, 3, 100);
    if (ret != ESP_OK) return ret;

    data->voc_raw = (buf[0] << 8) | buf[1];

    // VOC index algorithm (simplified - should use Sensirion's algorithm)
    // Map raw signal to 0-500 index
    data->voc_index = (data->voc_raw * 500) / 65535;

    data->sgp40_valid = true;
    return ESP_OK;
}

/**
 * @brief Read BH1750 ambient light
 */
static esp_err_t read_bh1750(sensor_data_t *data)
{
    if (!s_hub.dev_bh1750) return ESP_ERR_INVALID_STATE;

    uint8_t buf[2];
    esp_err_t ret = i2c_master_receive(s_hub.dev_bh1750, buf, 2, 100);
    if (ret != ESP_OK) return ret;

    uint16_t raw = (buf[0] << 8) | buf[1];
    data->lux = (float)raw / 1.2f;  // Default mode resolution

    data->bh1750_valid = true;
    return ESP_OK;
}

// ============================================================================
// UART Sensor Handlers
// ============================================================================

/**
 * @brief LD2410 UART receive task
 */
static void ld2410_rx_task(void *pvParameters)
{
    uart_config_t uart_config = {
        .baud_rate = CONFIG_LD2410_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    uart_param_config(CONFIG_LD2410_UART_PORT, &uart_config);
    uart_set_pin(CONFIG_LD2410_UART_PORT,
                 CONFIG_LD2410_UART_TX_GPIO,
                 CONFIG_LD2410_UART_RX_GPIO,
                 UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE);
    uart_driver_install(CONFIG_LD2410_UART_PORT, 256, 0, 0, NULL, 0);

    s_hub.status.ld2410_present = true;

    uint8_t buf[64];
    while (1) {
        int len = uart_read_bytes(CONFIG_LD2410_UART_PORT, buf, sizeof(buf),
                                  pdMS_TO_TICKS(100));
        if (len > 0) {
            // Parse LD2410 protocol
            // Frame format: F4 F3 F2 F1 [header] [data] F8 F7 F6 F5
            // TODO: Implement full protocol parsing
        }
    }
}

/**
 * @brief SEN0540 UART receive task
 */
static void sen0540_rx_task(void *pvParameters)
{
    uart_config_t uart_config = {
        .baud_rate = CONFIG_SEN0540_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    uart_param_config(CONFIG_SEN0540_UART_PORT, &uart_config);
    uart_set_pin(CONFIG_SEN0540_UART_PORT,
                 CONFIG_SEN0540_UART_TX_GPIO,
                 CONFIG_SEN0540_UART_RX_GPIO,
                 UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE);
    uart_driver_install(CONFIG_SEN0540_UART_PORT, 256, 0, 0, NULL, 0);

    s_hub.status.sen0540_present = true;

    uint8_t buf[32];
    while (1) {
        int len = uart_read_bytes(CONFIG_SEN0540_UART_PORT, buf, sizeof(buf),
                                  pdMS_TO_TICKS(100));
        if (len > 0) {
            // Parse SEN0540 response
            // Command ID is typically in first byte
            // TODO: Integrate with command_cache
        }
    }
}

// ============================================================================
// Public API Implementation
// ============================================================================

esp_err_t sensor_hub_init(void)
{
    if (s_hub.initialized) {
        ESP_LOGW(TAG, "Sensor hub already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing sensor hub...");

    // Create mutex
    s_hub.mutex = xSemaphoreCreateMutex();
    if (!s_hub.mutex) {
        return ESP_ERR_NO_MEM;
    }

    // Initialize I2C bus
    i2c_master_bus_config_t i2c_config = {
        .i2c_port = CONFIG_SENSOR_I2C_PORT,
        .sda_io_num = CONFIG_SENSOR_I2C_SDA_GPIO,
        .scl_io_num = CONFIG_SENSOR_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&i2c_config, &s_hub.i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // Probe and initialize each I2C sensor
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = CONFIG_SENSOR_I2C_FREQ_HZ,
    };

    // SHT40
    dev_config.device_address = SHT40_ADDR;
    if (i2c_master_bus_add_device(s_hub.i2c_bus, &dev_config, &s_hub.dev_sht40) == ESP_OK) {
        s_hub.status.sht40_present = true;
        ESP_LOGI(TAG, "SHT40 found at 0x%02X", SHT40_ADDR);
    }

    // BMP388
    dev_config.device_address = BMP388_ADDR;
    if (i2c_master_bus_add_device(s_hub.i2c_bus, &dev_config, &s_hub.dev_bmp388) == ESP_OK) {
        // Verify chip ID
        uint8_t chip_id;
        if (i2c_read_reg(s_hub.dev_bmp388, BMP388_REG_CHIP_ID, &chip_id, 1) == ESP_OK) {
            if (chip_id == BMP388_CHIP_ID) {
                s_hub.status.bmp388_present = true;
                // Configure for normal mode
                i2c_write_reg(s_hub.dev_bmp388, BMP388_REG_PWR_CTRL, 0x33);
                ESP_LOGI(TAG, "BMP388 found at 0x%02X", BMP388_ADDR);
            }
        }
    }

    // SCD41
    dev_config.device_address = SCD41_ADDR;
    if (i2c_master_bus_add_device(s_hub.i2c_bus, &dev_config, &s_hub.dev_scd41) == ESP_OK) {
        // Start periodic measurement
        if (i2c_write_cmd(s_hub.dev_scd41, SCD41_CMD_START_MEASUREMENT) == ESP_OK) {
            s_hub.status.scd41_present = true;
            ESP_LOGI(TAG, "SCD41 found at 0x%02X", SCD41_ADDR);
        }
    }

    // ENS160
    dev_config.device_address = ENS160_ADDR;
    if (i2c_master_bus_add_device(s_hub.i2c_bus, &dev_config, &s_hub.dev_ens160) == ESP_OK) {
        // Verify part ID
        uint8_t part_id[2];
        if (i2c_read_reg(s_hub.dev_ens160, ENS160_REG_PART_ID, part_id, 2) == ESP_OK) {
            if ((part_id[0] | (part_id[1] << 8)) == ENS160_PART_ID) {
                s_hub.status.ens160_present = true;
                // Set to standard operation mode
                i2c_write_reg(s_hub.dev_ens160, ENS160_REG_OPMODE, 0x02);
                ESP_LOGI(TAG, "ENS160 found at 0x%02X", ENS160_ADDR);
            }
        }
    }

    // SGP40
    dev_config.device_address = SGP40_ADDR;
    if (i2c_master_bus_add_device(s_hub.i2c_bus, &dev_config, &s_hub.dev_sgp40) == ESP_OK) {
        s_hub.status.sgp40_present = true;
        ESP_LOGI(TAG, "SGP40 found at 0x%02X", SGP40_ADDR);
    }

    // BH1750
    dev_config.device_address = BH1750_ADDR;
    if (i2c_master_bus_add_device(s_hub.i2c_bus, &dev_config, &s_hub.dev_bh1750) == ESP_OK) {
        // Power on and set continuous high-res mode
        uint8_t cmd = BH1750_CMD_POWER_ON;
        i2c_master_transmit(s_hub.dev_bh1750, &cmd, 1, 100);
        cmd = BH1750_CMD_CONTINUOUS_HIGH;
        if (i2c_master_transmit(s_hub.dev_bh1750, &cmd, 1, 100) == ESP_OK) {
            s_hub.status.bh1750_present = true;
            ESP_LOGI(TAG, "BH1750 found at 0x%02X", BH1750_ADDR);
        }
    }

    // Start UART sensor tasks
    xTaskCreate(ld2410_rx_task, "ld2410", 2048, NULL, 2, NULL);
    xTaskCreate(sen0540_rx_task, "sen0540", 2048, NULL, 2, NULL);

    s_hub.initialized = true;

    ESP_LOGI(TAG, "Sensor hub initialized. I2C: %d/%d, UART: 2",
             s_hub.status.sht40_present + s_hub.status.bmp388_present +
             s_hub.status.scd41_present + s_hub.status.ens160_present +
             s_hub.status.sgp40_present + s_hub.status.bh1750_present,
             6);

    return ESP_OK;
}

void sensor_hub_deinit(void)
{
    if (!s_hub.initialized) return;

    // Clean up I2C devices
    if (s_hub.i2c_bus) {
        i2c_del_master_bus(s_hub.i2c_bus);
    }

    // Clean up UARTs
    uart_driver_delete(CONFIG_LD2410_UART_PORT);
    uart_driver_delete(CONFIG_SEN0540_UART_PORT);

    if (s_hub.mutex) {
        vSemaphoreDelete(s_hub.mutex);
    }

    memset(&s_hub, 0, sizeof(s_hub));
}

esp_err_t sensor_hub_read_all(sensor_data_t *data)
{
    if (!s_hub.initialized || !data) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_hub.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    memset(data, 0, sizeof(sensor_data_t));
    data->timestamp_us = esp_timer_get_time();

    // Read all sensors (ignore individual errors)
    if (s_hub.status.sht40_present) {
        if (read_sht40(data) != ESP_OK) s_hub.status.i2c_errors++;
    }

    if (s_hub.status.bmp388_present) {
        if (read_bmp388(data) != ESP_OK) s_hub.status.i2c_errors++;
    }

    if (s_hub.status.scd41_present) {
        if (read_scd41(data) != ESP_OK) s_hub.status.i2c_errors++;
    }

    if (s_hub.status.ens160_present) {
        if (read_ens160(data) != ESP_OK) s_hub.status.i2c_errors++;
    }

    if (s_hub.status.sgp40_present) {
        if (read_sgp40(data) != ESP_OK) s_hub.status.i2c_errors++;
    }

    if (s_hub.status.bh1750_present) {
        if (read_bh1750(data) != ESP_OK) s_hub.status.i2c_errors++;
    }

    s_hub.status.read_count++;
    xSemaphoreGive(s_hub.mutex);

    return ESP_OK;
}

void sensor_hub_get_status(sensor_status_t *status)
{
    if (status) {
        memcpy(status, &s_hub.status, sizeof(sensor_status_t));
    }
}

int sensor_hub_to_json(const sensor_data_t *data, char *json_buf, size_t buf_len)
{
    if (!data || !json_buf || buf_len == 0) return -1;

    return snprintf(json_buf, buf_len,
        "{"
        "\"timestamp\":%lld,"
        "\"temperature\":%.2f,"
        "\"humidity\":%.1f,"
        "\"pressure\":%.2f,"
        "\"altitude\":%.1f,"
        "\"co2\":%d,"
        "\"tvoc\":%d,"
        "\"eco2\":%d,"
        "\"aqi\":%d,"
        "\"voc_index\":%ld,"
        "\"lux\":%.1f,"
        "\"presence\":%d,"
        "\"presence_distance\":%d"
        "}",
        (long long)data->timestamp_us / 1000,
        data->temperature,
        data->humidity,
        data->pressure,
        data->altitude,
        data->co2,
        data->tvoc,
        data->eco2,
        data->aqi,
        (long)data->voc_index,
        data->lux,
        data->presence,
        data->presence_distance
    );
}

const char *sensor_hub_aqi_description(air_quality_level_t aqi)
{
    switch (aqi) {
        case AIR_QUALITY_EXCELLENT: return "Excellent";
        case AIR_QUALITY_GOOD:      return "Good";
        case AIR_QUALITY_MODERATE:  return "Moderate";
        case AIR_QUALITY_POOR:      return "Poor";
        case AIR_QUALITY_UNHEALTHY: return "Unhealthy";
        default:                    return "Unknown";
    }
}

bool sensor_hub_is_initialized(void)
{
    return s_hub.initialized;
}
