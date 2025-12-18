/**
 * @file sensor_hub.c
 * @brief Phase A: Multi-Sensor Integration Hub Implementation
 *
 * This module coordinates all environmental and interaction sensors,
 * providing a unified thread-safe interface for the UI layer.
 *
 * Task Architecture:
 *   - Environment Task (Core 0, 5Hz): SEN66, VEML7700, SEN0395
 *   - Reactive Task (Core 0, 33Hz): PAJ7620U2, VL53L1X
 *
 * Both tasks share data via mutex-protected global structure.
 */

#include "sensor_hub.h"
#include "drivers/sen66.h"
#include "drivers/sen0395.h"
#include "drivers/vl53l1x.h"
#include "drivers/paj7620.h"
#include "drivers/veml7700.h"

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/i2c.h"

static const char *TAG = "SENSOR_HUB";

// Task handles
static TaskHandle_t s_env_task_handle = NULL;
static TaskHandle_t s_reactive_task_handle = NULL;

// Synchronization
static SemaphoreHandle_t s_data_mutex = NULL;
static EventGroupHandle_t s_event_group = NULL;

// Global sensor data
static room_environment_t s_room_data = {0};

// Callbacks
static gesture_callback_t s_gesture_callback = NULL;
static void *s_gesture_callback_data = NULL;
static proximity_callback_t s_proximity_callback = NULL;
static void *s_proximity_callback_data = NULL;
static presence_callback_t s_presence_callback = NULL;
static void *s_presence_callback_data = NULL;

// Configuration
static sensor_hub_config_t s_config;
static bool s_initialized = false;

// Previous states for change detection
static bool s_prev_is_present = false;
static bool s_prev_is_close = false;
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
 * @brief Calculate AQI level from sensor data
 */
static aqi_level_t calculate_aqi_level(const air_quality_data_t *air)
{
    // Simple AQI calculation based on PM2.5 (primary) and CO2
    float pm25 = air->pm2_5;
    float co2 = air->co2_ppm;

    // PM2.5 based level
    aqi_level_t pm_level;
    if (pm25 <= 12.0f) pm_level = AQI_GOOD;
    else if (pm25 <= 35.4f) pm_level = AQI_MODERATE;
    else if (pm25 <= 55.4f) pm_level = AQI_UNHEALTHY_SENSITIVE;
    else if (pm25 <= 150.4f) pm_level = AQI_UNHEALTHY;
    else if (pm25 <= 250.4f) pm_level = AQI_VERY_UNHEALTHY;
    else pm_level = AQI_HAZARDOUS;

    // CO2 based level
    aqi_level_t co2_level;
    if (co2 <= 800.0f) co2_level = AQI_GOOD;
    else if (co2 <= 1000.0f) co2_level = AQI_MODERATE;
    else if (co2 <= 1500.0f) co2_level = AQI_UNHEALTHY_SENSITIVE;
    else if (co2 <= 2000.0f) co2_level = AQI_UNHEALTHY;
    else if (co2 <= 3000.0f) co2_level = AQI_VERY_UNHEALTHY;
    else co2_level = AQI_HAZARDOUS;

    // Return worst level
    return (pm_level > co2_level) ? pm_level : co2_level;
}

/**
 * @brief Environment sensor task (500ms cycle)
 *
 * Reads: SEN66 (air quality), VEML7700 (light), SEN0395 (presence)
 */
static void environment_sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Environment sensor task started");

    // Initialize SEN66
    if (sen66_init(I2C_NUM_0) == ESP_OK) {
        sen66_start_measurement();
        s_room_data.sensor_status.sen66_online = true;
    } else {
        ESP_LOGW(TAG, "SEN66 not available");
    }

    // Initialize VEML7700
    veml7700_config_t veml_config = VEML7700_DEFAULT_CONFIG();
    if (veml7700_init(&veml_config) == ESP_OK) {
        s_room_data.sensor_status.veml7700_online = true;
    } else {
        ESP_LOGW(TAG, "VEML7700 not available");
    }

    // Initialize SEN0395
    sen0395_config_t radar_config = SEN0395_DEFAULT_CONFIG();
    radar_config.uart_num = s_config.uart_num;
    radar_config.tx_pin = s_config.uart_tx_pin;
    radar_config.rx_pin = s_config.uart_rx_pin;
    if (sen0395_init(&radar_config) == ESP_OK) {
        s_room_data.sensor_status.sen0395_online = true;
    } else {
        ESP_LOGW(TAG, "SEN0395 not available");
    }

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t cycle_time = pdMS_TO_TICKS(500);

    while (1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // === Read SEN66 (Air Quality) ===
        if (s_room_data.sensor_status.sen66_online) {
            bool data_ready = false;
            if (sen66_is_data_ready(&data_ready) == ESP_OK && data_ready) {
                sen66_data_t sen66_data;
                if (sen66_read_data(&sen66_data) == ESP_OK && sen66_data.data_valid) {
                    xSemaphoreTake(s_data_mutex, portMAX_DELAY);

                    s_room_data.air.co2_ppm = sen66_data.co2;
                    s_room_data.air.pm1_0 = sen66_data.pm1_0;
                    s_room_data.air.pm2_5 = sen66_data.pm2_5;
                    s_room_data.air.pm4_0 = sen66_data.pm4_0;
                    s_room_data.air.pm10 = sen66_data.pm10;
                    s_room_data.air.voc_index = sen66_data.voc_index;
                    s_room_data.air.nox_index = sen66_data.nox_index;
                    s_room_data.air.temperature = sen66_data.temperature;
                    s_room_data.air.humidity = sen66_data.humidity;
                    s_room_data.air.valid = true;
                    s_room_data.air.timestamp_ms = now;
                    s_room_data.air.aqi_level = calculate_aqi_level(&s_room_data.air);

                    xSemaphoreGive(s_data_mutex);
                    xEventGroupSetBits(s_event_group, SENSOR_AIR_QUALITY_BIT);
                }
            }
        }

        // === Read VEML7700 (Light) ===
        if (s_room_data.sensor_status.veml7700_online) {
            veml7700_data_t light_data;
            if (veml7700_read_data(&light_data) == ESP_OK) {
                xSemaphoreTake(s_data_mutex, portMAX_DELAY);

                uint32_t prev_lux = s_room_data.light.lux;
                s_room_data.light.lux = light_data.lux;
                s_room_data.light.white_level = light_data.white_raw;
                s_room_data.light.is_dark = (light_data.lux < s_config.darkness_threshold_lux);
                s_room_data.light.is_bright = (light_data.lux > s_config.brightness_threshold_lux);
                s_room_data.light.timestamp_ms = now;

                // Check for significant light change (>20%)
                if (prev_lux > 0) {
                    float change = (float)abs((int)light_data.lux - (int)prev_lux) / (float)prev_lux;
                    if (change > 0.2f) {
                        xEventGroupSetBits(s_event_group, SENSOR_LIGHT_CHANGED_BIT);
                    }
                }

                xSemaphoreGive(s_data_mutex);
            }
        }

        // === Read SEN0395 (Presence) ===
        if (s_room_data.sensor_status.sen0395_online) {
            sen0395_data_t radar_data;
            if (sen0395_read_data(&radar_data) == ESP_OK) {
                xSemaphoreTake(s_data_mutex, portMAX_DELAY);

                s_room_data.presence.is_present = radar_data.is_present;
                s_room_data.presence.radar_distance_cm = radar_data.distance_cm;
                s_room_data.presence.motion_energy = radar_data.motion_energy;
                s_room_data.presence.static_energy = radar_data.static_energy;

                // Track presence duration
                if (radar_data.is_present && !s_prev_is_present) {
                    s_presence_start_time = now;
                }
                if (radar_data.is_present) {
                    s_room_data.presence.presence_duration_ms = now - s_presence_start_time;
                } else {
                    s_room_data.presence.presence_duration_ms = 0;
                }

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
        }

        s_room_data.system_uptime_ms = now;
        vTaskDelayUntil(&last_wake_time, cycle_time);
    }
}

/**
 * @brief Reactive sensor task (30ms cycle for fast response)
 *
 * Reads: PAJ7620U2 (gesture), VL53L1X (proximity/distance)
 */
static void reactive_sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Reactive sensor task started");

    // Initialize PAJ7620U2
    paj7620_config_t paj_config = PAJ7620_DEFAULT_CONFIG();
    if (paj7620_init(&paj_config) == ESP_OK) {
        s_room_data.sensor_status.paj7620_online = true;
    } else {
        ESP_LOGW(TAG, "PAJ7620U2 not available");
    }

    // Initialize VL53L1X
    vl53l1x_config_t tof_config = VL53L1X_DEFAULT_CONFIG();
    tof_config.timing_budget_ms = VL53L1X_TIMING_33MS;
    tof_config.inter_measurement_ms = 30;
    if (vl53l1x_init(&tof_config) == ESP_OK) {
        vl53l1x_start_ranging();
        s_room_data.sensor_status.vl53l1x_online = true;
    } else {
        ESP_LOGW(TAG, "VL53L1X not available");
    }

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t cycle_time = pdMS_TO_TICKS(30);

    while (1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // === Read PAJ7620U2 (Gesture) ===
        if (s_room_data.sensor_status.paj7620_online) {
            if (paj7620_is_gesture_available()) {
                paj7620_data_t gesture_data;
                if (paj7620_read_gesture(&gesture_data) == ESP_OK && gesture_data.gesture_detected) {
                    xSemaphoreTake(s_data_mutex, portMAX_DELAY);

                    s_room_data.gesture.last_gesture = (gesture_type_t)gesture_data.gesture;
                    s_room_data.gesture.gesture_count++;
                    s_room_data.gesture.last_gesture_time_ms = now;
                    s_room_data.gesture.gesture_pending = true;

                    xSemaphoreGive(s_data_mutex);
                    xEventGroupSetBits(s_event_group, SENSOR_GESTURE_EVENT_BIT);

                    ESP_LOGI(TAG, "Gesture detected: %s",
                             paj7620_gesture_to_string(gesture_data.gesture));

                    if (s_gesture_callback) {
                        s_gesture_callback((gesture_type_t)gesture_data.gesture,
                                          s_gesture_callback_data);
                    }
                }
            }
        }

        // === Read VL53L1X (ToF Distance) ===
        if (s_room_data.sensor_status.vl53l1x_online) {
            if (vl53l1x_is_data_ready()) {
                vl53l1x_data_t tof_data;
                if (vl53l1x_read_data(&tof_data) == ESP_OK && tof_data.valid) {
                    xSemaphoreTake(s_data_mutex, portMAX_DELAY);

                    uint16_t prev_distance = s_room_data.presence.tof_distance_mm;
                    s_room_data.presence.tof_distance_mm = tof_data.distance_mm;
                    s_room_data.presence.tof_valid = tof_data.valid;
                    s_room_data.presence.tof_signal_rate = (uint8_t)(tof_data.signal_rate / 100);
                    s_room_data.presence.timestamp_ms = now;

                    // Detect approaching/leaving
                    if (prev_distance > 0 && tof_data.distance_mm > 0) {
                        int16_t delta = (int16_t)prev_distance - (int16_t)tof_data.distance_mm;
                        s_room_data.presence.approaching = (delta > 50);
                        s_room_data.presence.leaving = (delta < -50);
                    }

                    // Proximity threshold check
                    bool is_close = (tof_data.distance_mm < s_config.proximity_threshold_mm);
                    if (is_close != s_prev_is_close) {
                        s_prev_is_close = is_close;
                        xEventGroupSetBits(s_event_group, SENSOR_PROXIMITY_EVENT_BIT);

                        if (s_proximity_callback) {
                            s_proximity_callback(is_close, tof_data.distance_mm,
                                               s_proximity_callback_data);
                        }
                    }

                    xSemaphoreGive(s_data_mutex);
                }
            }
        }

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

    // Create sensor tasks
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        environment_sensor_task,
        "env_sensors",
        4096,
        NULL,
        s_config.env_task_priority,
        &s_env_task_handle,
        s_config.env_task_core
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create environment task");
        return ESP_ERR_NO_MEM;
    }

    task_ret = xTaskCreatePinnedToCore(
        reactive_sensor_task,
        "reactive_sensors",
        4096,
        NULL,
        s_config.reactive_task_priority,
        &s_reactive_task_handle,
        s_config.reactive_task_core
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create reactive task");
        vTaskDelete(s_env_task_handle);
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Sensor hub initialized successfully");
    ESP_LOGI(TAG, "  Environment task: Core %d, Priority %d",
             s_config.env_task_core, s_config.env_task_priority);
    ESP_LOGI(TAG, "  Reactive task: Core %d, Priority %d",
             s_config.reactive_task_core, s_config.reactive_task_priority);

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
    if (s_reactive_task_handle) {
        vTaskDelete(s_reactive_task_handle);
        s_reactive_task_handle = NULL;
    }

    // Deinitialize sensors
    sen66_deinit();
    sen0395_deinit();
    vl53l1x_deinit();
    paj7620_deinit();
    veml7700_deinit();

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

esp_err_t sensor_hub_get_air_quality(air_quality_data_t *data)
{
    if (!s_initialized || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_data_mutex, portMAX_DELAY);
    memcpy(data, &s_room_data.air, sizeof(air_quality_data_t));
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

esp_err_t sensor_hub_get_gesture(gesture_type_t *gesture)
{
    if (!s_initialized || !gesture) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_data_mutex, portMAX_DELAY);

    if (!s_room_data.gesture.gesture_pending) {
        xSemaphoreGive(s_data_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    *gesture = s_room_data.gesture.last_gesture;
    s_room_data.gesture.gesture_pending = false;

    xSemaphoreGive(s_data_mutex);

    return ESP_OK;
}

esp_err_t sensor_hub_get_light(ambient_light_data_t *data)
{
    if (!s_initialized || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_data_mutex, portMAX_DELAY);
    memcpy(data, &s_room_data.light, sizeof(ambient_light_data_t));
    xSemaphoreGive(s_data_mutex);

    return ESP_OK;
}

esp_err_t sensor_hub_register_gesture_callback(gesture_callback_t callback, void *user_data)
{
    s_gesture_callback = callback;
    s_gesture_callback_data = user_data;
    return ESP_OK;
}

esp_err_t sensor_hub_register_proximity_callback(proximity_callback_t callback, void *user_data)
{
    s_proximity_callback = callback;
    s_proximity_callback_data = user_data;
    return ESP_OK;
}

esp_err_t sensor_hub_register_presence_callback(presence_callback_t callback, void *user_data)
{
    s_presence_callback = callback;
    s_presence_callback_data = user_data;
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

    if (strcmp(sensor_name, "sen66") == 0) {
        return s_room_data.sensor_status.sen66_online;
    } else if (strcmp(sensor_name, "sen0395") == 0) {
        return s_room_data.sensor_status.sen0395_online;
    } else if (strcmp(sensor_name, "vl53l1x") == 0) {
        return s_room_data.sensor_status.vl53l1x_online;
    } else if (strcmp(sensor_name, "paj7620") == 0) {
        return s_room_data.sensor_status.paj7620_online;
    } else if (strcmp(sensor_name, "veml7700") == 0) {
        return s_room_data.sensor_status.veml7700_online;
    }

    return false;
}

esp_err_t sensor_hub_force_update(void)
{
    // This would trigger immediate sensor readings
    // For now, just return OK as the tasks run continuously
    return ESP_OK;
}

const char *sensor_hub_gesture_to_string(gesture_type_t gesture)
{
    switch (gesture) {
        case GESTURE_UP: return "Up";
        case GESTURE_DOWN: return "Down";
        case GESTURE_LEFT: return "Left";
        case GESTURE_RIGHT: return "Right";
        case GESTURE_FORWARD: return "Forward";
        case GESTURE_BACKWARD: return "Backward";
        case GESTURE_CLOCKWISE: return "Clockwise";
        case GESTURE_COUNTERCLOCKWISE: return "Counter-clockwise";
        case GESTURE_WAVE: return "Wave";
        default: return "None";
    }
}

const char *sensor_hub_aqi_to_string(aqi_level_t level)
{
    switch (level) {
        case AQI_GOOD: return "Good";
        case AQI_MODERATE: return "Moderate";
        case AQI_UNHEALTHY_SENSITIVE: return "Unhealthy (Sensitive)";
        case AQI_UNHEALTHY: return "Unhealthy";
        case AQI_VERY_UNHEALTHY: return "Very Unhealthy";
        case AQI_HAZARDOUS: return "Hazardous";
        default: return "Unknown";
    }
}
