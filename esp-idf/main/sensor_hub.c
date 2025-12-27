/**
 * @file sensor_hub.c
 * @brief Omni-P4: Multi-Sensor Integration Hub Implementation
 *
 * Unified sensor management for ESP32-P4 Smart Home Console
 *
 * Task Architecture:
 *   - Environment Task (Core 0, 5s cycle): All I2C sensors
 *     - SCD41: CO2, Temperature, Humidity
 *     - SGP40: VOC Index
 *     - ENS160: eCO2, TVOC, AQI
 *     - BMP388: Barometric pressure, Altitude
 *     - SHT40: High-precision Temperature/Humidity
 *     - SEN0428: PM1.0, PM2.5, PM10
 *   - Presence Task (Core 0, 100ms): SEN0395 mmWave UART
 *   - Voice Task (Core 0, 50ms): SEN0540 voice recognition UART
 */

#include "sensor_hub.h"
#include "drivers/scd41.h"
#include "drivers/sgp40.h"
#include "drivers/ens160.h"
#include "drivers/bmp388.h"
#include "drivers/sht40.h"
#include "drivers/sen0428.h"
#include "drivers/sen0395.h"
#include "drivers/sen0540.h"

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/i2c.h"

static const char *TAG = "SENSOR_HUB";

// Task handles
static TaskHandle_t s_env_task_handle = NULL;
static TaskHandle_t s_presence_task_handle = NULL;
static TaskHandle_t s_voice_task_handle = NULL;

// Synchronization
static SemaphoreHandle_t s_data_mutex = NULL;
static EventGroupHandle_t s_event_group = NULL;

// Global sensor data
static room_environment_t s_room_data = {0};

// Callbacks
static presence_callback_t s_presence_callback = NULL;
static void *s_presence_callback_data = NULL;
static voice_cmd_callback_t s_voice_callback = NULL;
static void *s_voice_callback_data = NULL;

// Configuration
static sensor_hub_config_t s_config;
static bool s_initialized = false;

// Previous states for change detection
static bool s_prev_is_present = false;
static uint32_t s_presence_start_time = 0;

/**
 * @brief Initialize I2C master
 */
static esp_err_t sensor_hub_i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = s_config.i2c_sda_pin,
        .scl_io_num = s_config.i2c_scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = s_config.i2c_freq_hz,
    };

    esp_err_t ret = i2c_param_config(I2C_NUM_0, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2C initialized (SDA:%d, SCL:%d, %luHz)",
             s_config.i2c_sda_pin, s_config.i2c_scl_pin, s_config.i2c_freq_hz);

    return ESP_OK;
}

/**
 * @brief Environment sensor task (5000ms cycle)
 *
 * Reads all I2C sensors: SCD41, SGP40, ENS160, BMP388, SHT40, SEN0428
 */
static void environment_sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Environment sensor task started");

    // Initialize SCD41 (CO2/Temp/Humidity)
    scd41_config_t scd41_cfg = SCD41_DEFAULT_CONFIG();
    scd41_cfg.altitude_m = s_config.altitude_m;
    if (scd41_init(&scd41_cfg) == ESP_OK) {
        scd41_start_measurement();
        s_room_data.status.scd41_online = true;
        ESP_LOGI(TAG, "SCD41 initialized");
    } else {
        ESP_LOGW(TAG, "SCD41 not available");
    }

    // Initialize SGP40 (VOC)
    sgp40_config_t sgp40_cfg = SGP40_DEFAULT_CONFIG();
    if (sgp40_init(&sgp40_cfg) == ESP_OK) {
        s_room_data.status.sgp40_online = true;
        ESP_LOGI(TAG, "SGP40 initialized");
    } else {
        ESP_LOGW(TAG, "SGP40 not available");
    }

    // Initialize ENS160 (Air Quality)
    ens160_config_t ens160_cfg = ENS160_DEFAULT_CONFIG();
    if (ens160_init(&ens160_cfg) == ESP_OK) {
        ens160_set_mode(ENS160_MODE_STANDARD);
        s_room_data.status.ens160_online = true;
        ESP_LOGI(TAG, "ENS160 initialized");
    } else {
        ESP_LOGW(TAG, "ENS160 not available");
    }

    // Initialize BMP388 (Barometric)
    bmp388_config_t bmp388_cfg = BMP388_DEFAULT_CONFIG();
    if (bmp388_init(&bmp388_cfg) == ESP_OK) {
        s_room_data.status.bmp388_online = true;
        ESP_LOGI(TAG, "BMP388 initialized");
    } else {
        ESP_LOGW(TAG, "BMP388 not available");
    }

    // Initialize SHT40 (Precision Temp/Humidity)
    sht40_config_t sht40_cfg = SHT40_DEFAULT_CONFIG();
    if (sht40_init(&sht40_cfg) == ESP_OK) {
        s_room_data.status.sht40_online = true;
        ESP_LOGI(TAG, "SHT40 initialized");
    } else {
        ESP_LOGW(TAG, "SHT40 not available");
    }

    // Initialize SEN0428 (PM2.5)
    sen0428_config_t sen0428_cfg = SEN0428_DEFAULT_CONFIG();
    if (sen0428_init(&sen0428_cfg) == ESP_OK) {
        s_room_data.status.sen0428_online = true;
        ESP_LOGI(TAG, "SEN0428 initialized");
    } else {
        ESP_LOGW(TAG, "SEN0428 not available");
    }

    // Reference temperature and humidity for compensation
    float ref_temp = 25.0f;
    float ref_humidity = 50.0f;

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t cycle_time = pdMS_TO_TICKS(5000);

    while (1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // === Read SHT40 first (for compensation data) ===
        if (s_room_data.status.sht40_online) {
            sht40_data_t sht40_data;
            if (sht40_read_data(&sht40_data) == ESP_OK) {
                xSemaphoreTake(s_data_mutex, portMAX_DELAY);
                s_room_data.precision.temperature = sht40_data.temperature;
                s_room_data.precision.humidity = sht40_data.humidity;
                s_room_data.precision.timestamp_ms = now;
                ref_temp = sht40_data.temperature;
                ref_humidity = sht40_data.humidity;
                xSemaphoreGive(s_data_mutex);
            }
        }

        // === Read SCD41 (CO2/Temp/Humidity) ===
        if (s_room_data.status.scd41_online && scd41_is_data_ready()) {
            scd41_data_t scd41_data;
            if (scd41_read_data(&scd41_data) == ESP_OK && scd41_data.data_valid) {
                xSemaphoreTake(s_data_mutex, portMAX_DELAY);
                s_room_data.co2.co2_ppm = scd41_data.co2_ppm;
                s_room_data.co2.temperature = scd41_data.temperature;
                s_room_data.co2.humidity = scd41_data.humidity;
                s_room_data.co2.valid = true;
                s_room_data.co2.timestamp_ms = now;
                xSemaphoreGive(s_data_mutex);
            }
        }

        // === Read SGP40 (VOC Index) with compensation ===
        if (s_room_data.status.sgp40_online) {
            sgp40_data_t sgp40_data;
            if (sgp40_read_voc_index(&sgp40_data, ref_humidity, ref_temp) == ESP_OK) {
                xSemaphoreTake(s_data_mutex, portMAX_DELAY);
                s_room_data.voc.voc_index = sgp40_data.voc_index;
                s_room_data.voc.voc_raw = sgp40_data.voc_raw;
                s_room_data.voc.timestamp_ms = now;
                xSemaphoreGive(s_data_mutex);
            }
        }

        // === Read ENS160 (Air Quality) with compensation ===
        if (s_room_data.status.ens160_online) {
            // Set compensation values
            ens160_set_compensation(ref_temp, ref_humidity);

            ens160_data_t ens160_data;
            if (ens160_read_data(&ens160_data) == ESP_OK && ens160_data.data_valid) {
                xSemaphoreTake(s_data_mutex, portMAX_DELAY);
                s_room_data.ens_aqi.eco2 = ens160_data.eco2;
                s_room_data.ens_aqi.tvoc = ens160_data.tvoc;
                s_room_data.ens_aqi.aqi = (aqi_level_t)ens160_data.aqi;
                s_room_data.ens_aqi.timestamp_ms = now;
                xSemaphoreGive(s_data_mutex);

                // Set air quality event if poor
                if (ens160_data.aqi >= ENS160_AQI_POOR) {
                    xEventGroupSetBits(s_event_group, SENSOR_AIR_QUALITY_BIT);
                }
            }
        }

        // === Read BMP388 (Barometric) ===
        if (s_room_data.status.bmp388_online) {
            bmp388_data_t bmp388_data;
            if (bmp388_read_data(&bmp388_data) == ESP_OK) {
                xSemaphoreTake(s_data_mutex, portMAX_DELAY);

                float prev_pressure = s_room_data.baro.pressure;
                s_room_data.baro.pressure = bmp388_data.pressure;
                s_room_data.baro.temperature = bmp388_data.temperature;
                s_room_data.baro.altitude = bmp388_data.altitude;
                s_room_data.baro.timestamp_ms = now;

                // Detect significant pressure change (>1 hPa)
                if (prev_pressure > 0 &&
                    (bmp388_data.pressure - prev_pressure > 1.0f ||
                     prev_pressure - bmp388_data.pressure > 1.0f)) {
                    xEventGroupSetBits(s_event_group, SENSOR_PRESSURE_CHANGE_BIT);
                }

                xSemaphoreGive(s_data_mutex);
            }
        }

        // === Read SEN0428 (PM2.5) ===
        if (s_room_data.status.sen0428_online) {
            sen0428_data_t sen0428_data;
            if (sen0428_read_data(&sen0428_data) == ESP_OK) {
                xSemaphoreTake(s_data_mutex, portMAX_DELAY);
                s_room_data.pm.pm1_0 = sen0428_data.pm1_0_atm;
                s_room_data.pm.pm2_5 = sen0428_data.pm2_5_atm;
                s_room_data.pm.pm10 = sen0428_data.pm10_atm;
                s_room_data.pm.timestamp_ms = now;
                xSemaphoreGive(s_data_mutex);
            }
        }

        s_room_data.system_uptime_ms = now;
        xEventGroupSetBits(s_event_group, SENSOR_DATA_READY_BIT);

        vTaskDelayUntil(&last_wake_time, cycle_time);
    }
}

/**
 * @brief Presence detection task (100ms cycle)
 *
 * Reads SEN0395 mmWave radar via UART
 */
static void presence_sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Presence sensor task started");

    // Initialize SEN0395
    sen0395_config_t radar_cfg = SEN0395_DEFAULT_CONFIG();
    radar_cfg.uart_num = UART_NUM_1;
    radar_cfg.tx_pin = s_config.uart1_tx_pin;
    radar_cfg.rx_pin = s_config.uart1_rx_pin;

    if (sen0395_init(&radar_cfg) == ESP_OK) {
        s_room_data.status.sen0395_online = true;
        ESP_LOGI(TAG, "SEN0395 initialized");
    } else {
        ESP_LOGW(TAG, "SEN0395 not available");
        vTaskDelete(NULL);
        return;
    }

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t cycle_time = pdMS_TO_TICKS(100);

    while (1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // Process UART data
        sen0395_process();

        // Read presence data
        sen0395_data_t radar_data;
        if (sen0395_read_data(&radar_data) == ESP_OK) {
            xSemaphoreTake(s_data_mutex, portMAX_DELAY);

            s_room_data.presence.is_present = radar_data.is_present;
            s_room_data.presence.is_moving = radar_data.is_moving;
            s_room_data.presence.distance_cm = radar_data.distance_cm;

            // Track presence duration
            if (radar_data.is_present && !s_prev_is_present) {
                s_presence_start_time = now;
            }
            if (radar_data.is_present) {
                s_room_data.presence.presence_duration_ms = now - s_presence_start_time;
            } else {
                s_room_data.presence.presence_duration_ms = 0;
            }

            s_room_data.presence.timestamp_ms = now;

            // Presence change callback
            if (radar_data.is_present != s_prev_is_present) {
                s_prev_is_present = radar_data.is_present;
                xEventGroupSetBits(s_event_group, SENSOR_PRESENCE_EVENT_BIT);

                if (s_presence_callback) {
                    s_presence_callback(radar_data.is_present, s_presence_callback_data);
                }
            }

            xSemaphoreGive(s_data_mutex);
        }

        vTaskDelayUntil(&last_wake_time, cycle_time);
    }
}

/**
 * @brief Voice recognition callback wrapper
 */
static void voice_recognition_callback(uint8_t cmd_id, const char *keyword, void *user_data)
{
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    xSemaphoreTake(s_data_mutex, portMAX_DELAY);
    s_room_data.voice.cmd_id = cmd_id;
    strncpy(s_room_data.voice.keyword, keyword, sizeof(s_room_data.voice.keyword) - 1);
    s_room_data.voice.pending = true;
    s_room_data.voice.timestamp_ms = now;
    xSemaphoreGive(s_data_mutex);

    xEventGroupSetBits(s_event_group, SENSOR_VOICE_CMD_BIT);

    // Forward to user callback
    if (s_voice_callback) {
        s_voice_callback(cmd_id, keyword, s_voice_callback_data);
    }

    ESP_LOGI(TAG, "Voice command: ID=%d, Keyword='%s'", cmd_id, keyword);
}

/**
 * @brief Voice recognition task (50ms cycle)
 *
 * Processes SEN0540 voice recognition via UART
 */
static void voice_sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Voice recognition task started");

    // Initialize SEN0540
    sen0540_config_t voice_cfg = SEN0540_DEFAULT_CONFIG();
    voice_cfg.uart_num = UART_NUM_2;
    voice_cfg.tx_pin = s_config.uart2_tx_pin;
    voice_cfg.rx_pin = s_config.uart2_rx_pin;
    voice_cfg.callback = voice_recognition_callback;
    voice_cfg.callback_data = NULL;

    if (sen0540_init(&voice_cfg) == ESP_OK) {
        s_room_data.status.sen0540_online = true;
        ESP_LOGI(TAG, "SEN0540 initialized");
    } else {
        ESP_LOGW(TAG, "SEN0540 not available");
        vTaskDelete(NULL);
        return;
    }

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t cycle_time = pdMS_TO_TICKS(50);

    while (1) {
        // Process UART data (callbacks fire automatically)
        sen0540_process();

        vTaskDelayUntil(&last_wake_time, cycle_time);
    }
}

// =============================================================================
// Public API Implementation
// =============================================================================

esp_err_t sensor_hub_init(const sensor_hub_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Sensor hub already initialized");
        return ESP_OK;
    }

    if (!config) {
        sensor_hub_config_t default_config = SENSOR_HUB_DEFAULT_CONFIG();
        memcpy(&s_config, &default_config, sizeof(sensor_hub_config_t));
    } else {
        memcpy(&s_config, config, sizeof(sensor_hub_config_t));
    }

    // Create synchronization primitives
    s_data_mutex = xSemaphoreCreateMutex();
    if (!s_data_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    s_event_group = xEventGroupCreate();
    if (!s_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        vSemaphoreDelete(s_data_mutex);
        return ESP_ERR_NO_MEM;
    }

    // Initialize I2C
    esp_err_t ret = sensor_hub_i2c_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C initialization failed");
        return ret;
    }

    // Initialize room data
    memset(&s_room_data, 0, sizeof(room_environment_t));

    // Create environment sensor task (I2C sensors)
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        environment_sensor_task,
        "env_sensors",
        4096,
        NULL,
        s_config.task_priority,
        &s_env_task_handle,
        s_config.task_core
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create environment task");
        return ESP_ERR_NO_MEM;
    }

    // Create presence sensor task (UART1)
    task_ret = xTaskCreatePinnedToCore(
        presence_sensor_task,
        "presence_sensor",
        3072,
        NULL,
        s_config.task_priority + 1,
        &s_presence_task_handle,
        s_config.task_core
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create presence task");
        vTaskDelete(s_env_task_handle);
        return ESP_ERR_NO_MEM;
    }

    // Create voice recognition task (UART2)
    task_ret = xTaskCreatePinnedToCore(
        voice_sensor_task,
        "voice_sensor",
        3072,
        NULL,
        s_config.task_priority + 1,
        &s_voice_task_handle,
        s_config.task_core
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create voice task");
        vTaskDelete(s_env_task_handle);
        vTaskDelete(s_presence_task_handle);
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Sensor hub initialized successfully");
    ESP_LOGI(TAG, "  Environment task: Core %d, Priority %d (5s cycle)",
             s_config.task_core, s_config.task_priority);
    ESP_LOGI(TAG, "  Presence task: Core %d, Priority %d (100ms cycle)",
             s_config.task_core, s_config.task_priority + 1);
    ESP_LOGI(TAG, "  Voice task: Core %d, Priority %d (50ms cycle)",
             s_config.task_core, s_config.task_priority + 1);

    return ESP_OK;
}

esp_err_t sensor_hub_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    // Stop tasks
    if (s_env_task_handle) {
        vTaskDelete(s_env_task_handle);
        s_env_task_handle = NULL;
    }
    if (s_presence_task_handle) {
        vTaskDelete(s_presence_task_handle);
        s_presence_task_handle = NULL;
    }
    if (s_voice_task_handle) {
        vTaskDelete(s_voice_task_handle);
        s_voice_task_handle = NULL;
    }

    // Deinitialize sensors
    scd41_deinit();
    sgp40_deinit();
    ens160_deinit();
    bmp388_deinit();
    sht40_deinit();
    sen0428_deinit();
    sen0395_deinit();
    sen0540_deinit();

    // Release I2C
    i2c_driver_delete(I2C_NUM_0);

    // Delete synchronization primitives
    if (s_data_mutex) {
        vSemaphoreDelete(s_data_mutex);
        s_data_mutex = NULL;
    }
    if (s_event_group) {
        vEventGroupDelete(s_event_group);
        s_event_group = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "Sensor hub deinitialized");

    return ESP_OK;
}

esp_err_t sensor_hub_get_data(room_environment_t *data)
{
    if (!s_initialized || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_data_mutex, portMAX_DELAY);
    memcpy(data, &s_room_data, sizeof(room_environment_t));
    xSemaphoreGive(s_data_mutex);

    return ESP_OK;
}

esp_err_t sensor_hub_get_co2(co2_data_t *data)
{
    if (!s_initialized || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_data_mutex, portMAX_DELAY);
    memcpy(data, &s_room_data.co2, sizeof(co2_data_t));
    xSemaphoreGive(s_data_mutex);

    return ESP_OK;
}

esp_err_t sensor_hub_get_voc(voc_data_t *data)
{
    if (!s_initialized || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_data_mutex, portMAX_DELAY);
    memcpy(data, &s_room_data.voc, sizeof(voc_data_t));
    xSemaphoreGive(s_data_mutex);

    return ESP_OK;
}

esp_err_t sensor_hub_get_aqi(ens_air_quality_t *data)
{
    if (!s_initialized || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_data_mutex, portMAX_DELAY);
    memcpy(data, &s_room_data.ens_aqi, sizeof(ens_air_quality_t));
    xSemaphoreGive(s_data_mutex);

    return ESP_OK;
}

esp_err_t sensor_hub_get_barometric(barometric_data_t *data)
{
    if (!s_initialized || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_data_mutex, portMAX_DELAY);
    memcpy(data, &s_room_data.baro, sizeof(barometric_data_t));
    xSemaphoreGive(s_data_mutex);

    return ESP_OK;
}

esp_err_t sensor_hub_get_precision_env(precision_env_data_t *data)
{
    if (!s_initialized || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_data_mutex, portMAX_DELAY);
    memcpy(data, &s_room_data.precision, sizeof(precision_env_data_t));
    xSemaphoreGive(s_data_mutex);

    return ESP_OK;
}

esp_err_t sensor_hub_get_pm(pm_data_t *data)
{
    if (!s_initialized || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_data_mutex, portMAX_DELAY);
    memcpy(data, &s_room_data.pm, sizeof(pm_data_t));
    xSemaphoreGive(s_data_mutex);

    return ESP_OK;
}

esp_err_t sensor_hub_get_presence(presence_data_t *data)
{
    if (!s_initialized || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_data_mutex, portMAX_DELAY);
    memcpy(data, &s_room_data.presence, sizeof(presence_data_t));
    xSemaphoreGive(s_data_mutex);

    return ESP_OK;
}

esp_err_t sensor_hub_get_voice_cmd(voice_cmd_data_t *data)
{
    if (!s_initialized || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_data_mutex, portMAX_DELAY);

    if (!s_room_data.voice.pending) {
        xSemaphoreGive(s_data_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    memcpy(data, &s_room_data.voice, sizeof(voice_cmd_data_t));
    s_room_data.voice.pending = false;

    xSemaphoreGive(s_data_mutex);

    return ESP_OK;
}

esp_err_t sensor_hub_register_presence_callback(presence_callback_t callback, void *user_data)
{
    s_presence_callback = callback;
    s_presence_callback_data = user_data;
    return ESP_OK;
}

esp_err_t sensor_hub_register_voice_callback(voice_cmd_callback_t callback, void *user_data)
{
    s_voice_callback = callback;
    s_voice_callback_data = user_data;
    return ESP_OK;
}

EventGroupHandle_t sensor_hub_get_event_group(void)
{
    return s_event_group;
}

bool sensor_hub_is_sensor_online(const char *sensor_name)
{
    if (!s_initialized || !sensor_name) {
        return false;
    }

    if (strcmp(sensor_name, "scd41") == 0) {
        return s_room_data.status.scd41_online;
    } else if (strcmp(sensor_name, "sgp40") == 0) {
        return s_room_data.status.sgp40_online;
    } else if (strcmp(sensor_name, "ens160") == 0) {
        return s_room_data.status.ens160_online;
    } else if (strcmp(sensor_name, "bmp388") == 0) {
        return s_room_data.status.bmp388_online;
    } else if (strcmp(sensor_name, "sht40") == 0) {
        return s_room_data.status.sht40_online;
    } else if (strcmp(sensor_name, "sen0428") == 0) {
        return s_room_data.status.sen0428_online;
    } else if (strcmp(sensor_name, "sen0395") == 0) {
        return s_room_data.status.sen0395_online;
    } else if (strcmp(sensor_name, "sen0540") == 0) {
        return s_room_data.status.sen0540_online;
    }

    return false;
}

const char *sensor_hub_aqi_to_string(aqi_level_t level)
{
    switch (level) {
        case AQI_EXCELLENT: return "Excellent";
        case AQI_GOOD: return "Good";
        case AQI_MODERATE: return "Moderate";
        case AQI_POOR: return "Poor";
        case AQI_UNHEALTHY: return "Unhealthy";
        default: return "Unknown";
    }
}
