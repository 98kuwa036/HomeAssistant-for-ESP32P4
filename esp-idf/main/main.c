/**
 * ESP32-P4 Smart Home Console - Main Application
 *
 * Hardware Configuration (ESP32-P4-Function-EV-Board v1.5):
 *   - 7" MIPI-DSI Display (800x480)
 *   - ReSpeaker USB Mic Array (XVF3800) → USB Host
 *   - ES8311 DAC → DFRobot 2x3W Amp (DFR0119) → Speakers
 *   - Gravity I2C Hub (SEN0219) → Multiple Sensors:
 *     - SEN66: Air quality (CO2, PM2.5, VOC, NOx, Temp, Humidity)
 *     - VL53L1X: ToF distance sensor
 *     - PAJ7620U2: Gesture recognition
 *     - VEML7700: Ambient light
 *   - SEN0395: mmWave radar (UART) → Human presence
 *   - Gravity IR Transmitter (SEN0225) → Appliance control
 *   - OV9732 MIPI Camera → Face recognition (future)
 *
 * Software Architecture:
 *   ┌─────────────────────────────────────────────────────────┐
 *   │                    Application Layer                     │
 *   │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────────┐ │
 *   │  │  Audio  │  │   UI    │  │   HA    │  │   Sensor    │ │
 *   │  │  Task   │  │  Task   │  │  Task   │  │    Hub      │ │
 *   │  └────┬────┘  └────┬────┘  └────┬────┘  └──────┬──────┘ │
 *   └───────┼────────────┼───────────┼───────────────┼────────┘
 *           │            │           │               │
 *   ┌───────┴────────────┴───────────┴───────────────┴────────┐
 *   │                    Hardware Abstraction                  │
 *   │  USB Audio │ MIPI-DSI │ WiFi/HTTP │ I2C/UART Sensors    │
 *   └─────────────────────────────────────────────────────────┘
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "usb_audio.h"
#include "i2s_output.h"
#include "wifi_manager.h"
#include "ha_client.h"
#include "sensor_hub.h"

static const char *TAG = "MAIN";

// =============================================================================
// Hardware Pin Definitions (ESP32-P4-Function-EV-Board v1.5)
// =============================================================================

// Status LED
#define LED_STATUS_PIN          GPIO_NUM_47

// Boot Button
#define BUTTON_BOOT_PIN         GPIO_NUM_0

// I2S Pins (ES8311 Audio Codec)
#define I2S_MCLK_PIN            GPIO_NUM_13
#define I2S_BCLK_PIN            GPIO_NUM_12
#define I2S_WS_PIN              GPIO_NUM_10
#define I2S_DOUT_PIN            GPIO_NUM_9
#define I2S_DIN_PIN             GPIO_NUM_11

// I2C Pins (Gravity Hub → Multiple Sensors)
#define I2C_SDA_PIN             GPIO_NUM_7
#define I2C_SCL_PIN             GPIO_NUM_8

// UART Pins (SEN0395 mmWave Radar)
#define UART_RADAR_TX_PIN       GPIO_NUM_17
#define UART_RADAR_RX_PIN       GPIO_NUM_18

// IR Transmitter
#define IR_TX_PIN               GPIO_NUM_48

// =============================================================================
// Application State Machine
// =============================================================================

typedef enum {
    APP_STATE_BOOTING,          // System initialization
    APP_STATE_IDLE,             // Normal operation, display off/dim
    APP_STATE_ACTIVE,           // User present, display on
    APP_STATE_LISTENING,        // Voice input active
    APP_STATE_PROCESSING,       // Processing voice command
    APP_STATE_SPEAKING,         // TTS playback
    APP_STATE_ERROR,            // Error state
} app_state_t;

static app_state_t s_app_state = APP_STATE_BOOTING;
static SemaphoreHandle_t s_state_mutex = NULL;

// =============================================================================
// Event Group Bits
// =============================================================================

static EventGroupHandle_t s_app_event_group = NULL;

#define EVT_WIFI_CONNECTED      BIT0
#define EVT_HA_CONNECTED        BIT1
#define EVT_USB_AUDIO_READY     BIT2
#define EVT_DISPLAY_READY       BIT3
#define EVT_SENSORS_READY       BIT4
#define EVT_WAKE_WORD           BIT5
#define EVT_USER_PRESENT        BIT6
#define EVT_USER_APPROACHING    BIT7

// =============================================================================
// Display State
// =============================================================================

static bool s_display_on = false;
static uint32_t s_last_activity_ms = 0;
#define DISPLAY_TIMEOUT_MS      (30 * 1000)  // 30 seconds

// =============================================================================
// Utility Functions
// =============================================================================

static void set_app_state(app_state_t new_state)
{
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    if (s_app_state != new_state) {
        ESP_LOGI(TAG, "State: %d → %d", s_app_state, new_state);
        s_app_state = new_state;
    }
    xSemaphoreGive(s_state_mutex);
}

static app_state_t get_app_state(void)
{
    app_state_t state;
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    state = s_app_state;
    xSemaphoreGive(s_state_mutex);
    return state;
}

static void update_activity(void)
{
    s_last_activity_ms = esp_timer_get_time() / 1000;
}

static void set_led_pattern(app_state_t state)
{
    switch (state) {
        case APP_STATE_BOOTING:
            gpio_set_level(LED_STATUS_PIN, 1);
            break;
        case APP_STATE_IDLE:
            gpio_set_level(LED_STATUS_PIN, 0);
            break;
        case APP_STATE_ACTIVE:
            gpio_set_level(LED_STATUS_PIN, 1);
            break;
        case APP_STATE_LISTENING:
            // TODO: Breathing effect
            gpio_set_level(LED_STATUS_PIN, 1);
            break;
        case APP_STATE_PROCESSING:
            // TODO: Fast blink
            gpio_set_level(LED_STATUS_PIN, 1);
            break;
        case APP_STATE_SPEAKING:
            gpio_set_level(LED_STATUS_PIN, 1);
            break;
        case APP_STATE_ERROR:
            gpio_set_level(LED_STATUS_PIN, 1);
            break;
    }
}

// =============================================================================
// Sensor Callbacks
// =============================================================================

/**
 * @brief Gesture detection callback
 */
static void on_gesture_detected(gesture_type_t gesture, void *user_data)
{
    ESP_LOGI(TAG, "Gesture: %s", sensor_hub_gesture_to_string(gesture));
    update_activity();

    switch (gesture) {
        case GESTURE_UP:
            // Volume up or scroll up
            ESP_LOGI(TAG, "Action: Volume Up / Scroll Up");
            break;
        case GESTURE_DOWN:
            // Volume down or scroll down
            ESP_LOGI(TAG, "Action: Volume Down / Scroll Down");
            break;
        case GESTURE_LEFT:
            // Previous screen/item
            ESP_LOGI(TAG, "Action: Previous");
            break;
        case GESTURE_RIGHT:
            // Next screen/item
            ESP_LOGI(TAG, "Action: Next");
            break;
        case GESTURE_FORWARD:
            // Select/Confirm
            ESP_LOGI(TAG, "Action: Select");
            break;
        case GESTURE_BACKWARD:
            // Back/Cancel
            ESP_LOGI(TAG, "Action: Back");
            break;
        case GESTURE_WAVE:
            // Wake display or start voice
            ESP_LOGI(TAG, "Action: Wake");
            if (!s_display_on) {
                // TODO: Turn on display
            }
            break;
        default:
            break;
    }
}

/**
 * @brief Proximity change callback (VL53L1X)
 */
static void on_proximity_changed(bool is_close, uint16_t distance_mm, void *user_data)
{
    ESP_LOGI(TAG, "Proximity: %s (distance: %d mm)",
             is_close ? "CLOSE" : "FAR", distance_mm);

    if (is_close) {
        update_activity();
        xEventGroupSetBits(s_app_event_group, EVT_USER_APPROACHING);

        // Wake display when user approaches
        if (!s_display_on) {
            ESP_LOGI(TAG, "User approaching - waking display");
            // TODO: Display wake
            s_display_on = true;
        }
    }
}

/**
 * @brief Presence change callback (SEN0395 mmWave)
 */
static void on_presence_changed(bool is_present, void *user_data)
{
    ESP_LOGI(TAG, "Presence: %s", is_present ? "DETECTED" : "NONE");

    if (is_present) {
        xEventGroupSetBits(s_app_event_group, EVT_USER_PRESENT);
        set_app_state(APP_STATE_ACTIVE);
    } else {
        xEventGroupClearBits(s_app_event_group, EVT_USER_PRESENT);

        // Allow display to turn off after timeout
        if (get_app_state() == APP_STATE_ACTIVE) {
            set_app_state(APP_STATE_IDLE);
        }
    }
}

// =============================================================================
// Button Handler
// =============================================================================

static void IRAM_ATTR button_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xEventGroupSetBitsFromISR(s_app_event_group, EVT_WAKE_WORD, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// =============================================================================
// Initialization Functions
// =============================================================================

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

static esp_err_t init_gpio(void)
{
    // Status LED
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_STATUS_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&led_conf));

    // Boot Button with interrupt
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BUTTON_BOOT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&btn_conf));
    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add(BUTTON_BOOT_PIN, button_isr_handler, NULL);

    // IR Transmitter (output, active low)
    gpio_config_t ir_conf = {
        .pin_bit_mask = (1ULL << IR_TX_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&ir_conf));
    gpio_set_level(IR_TX_PIN, 0);

    return ESP_OK;
}

static esp_err_t init_audio(void)
{
    // USB Audio (ReSpeaker XVF3800)
    usb_audio_config_t usb_config = {
        .sample_rate = 16000,
        .channels = 1,
        .bits_per_sample = 16,
    };

    esp_err_t ret = usb_audio_init(&usb_config);
    if (ret == ESP_OK) {
        xEventGroupSetBits(s_app_event_group, EVT_USB_AUDIO_READY);
        ESP_LOGI(TAG, "USB Audio initialized");
    } else {
        ESP_LOGW(TAG, "USB Audio init failed: %s", esp_err_to_name(ret));
    }

    // I2S Output (ES8311 → DFRobot Amp)
    i2s_output_config_t i2s_config = {
        .mclk_pin = I2S_MCLK_PIN,
        .bclk_pin = I2S_BCLK_PIN,
        .ws_pin = I2S_WS_PIN,
        .dout_pin = I2S_DOUT_PIN,
        .sample_rate = 16000,
        .bits_per_sample = 16,
    };

    ret = i2s_output_init(&i2s_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S Output init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "I2S Output initialized");

    return ESP_OK;
}

static esp_err_t init_sensors(void)
{
    sensor_hub_config_t config = {
        .i2c_sda_pin = I2C_SDA_PIN,
        .i2c_scl_pin = I2C_SCL_PIN,
        .i2c_freq_hz = 100000,
        .uart_tx_pin = UART_RADAR_TX_PIN,
        .uart_rx_pin = UART_RADAR_RX_PIN,
        .uart_num = 1,
        .uart_baud_rate = 115200,
        .env_task_core = 0,
        .reactive_task_core = 0,
        .env_task_priority = 5,
        .reactive_task_priority = 10,
        .proximity_threshold_mm = 500,
        .darkness_threshold_lux = 10,
        .brightness_threshold_lux = 500,
    };

    esp_err_t ret = sensor_hub_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Sensor hub init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register callbacks
    sensor_hub_register_gesture_callback(on_gesture_detected, NULL);
    sensor_hub_register_proximity_callback(on_proximity_changed, NULL);
    sensor_hub_register_presence_callback(on_presence_changed, NULL);

    xEventGroupSetBits(s_app_event_group, EVT_SENSORS_READY);
    ESP_LOGI(TAG, "Sensor hub initialized with callbacks");

    return ESP_OK;
}

static esp_err_t init_display(void)
{
    // TODO: Initialize 7" MIPI-DSI display (800x480)
    // This will be implemented in Phase B with LVGL

    ESP_LOGI(TAG, "Display initialization (placeholder)");
    xEventGroupSetBits(s_app_event_group, EVT_DISPLAY_READY);

    return ESP_OK;
}

static esp_err_t init_network(void)
{
    esp_err_t ret = wifi_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi manager init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "WiFi manager initialized");

    return ESP_OK;
}

// =============================================================================
// Application Tasks
// =============================================================================

/**
 * @brief Audio processing task
 *
 * Handles voice input from ReSpeaker and TTS output.
 */
static void audio_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Audio task started (Core %d)", xPortGetCoreID());

    // Wait for audio hardware
    xEventGroupWaitBits(s_app_event_group, EVT_USB_AUDIO_READY,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    uint8_t audio_buffer[1024];
    size_t bytes_read;

    while (1) {
        EventBits_t bits = xEventGroupWaitBits(
            s_app_event_group,
            EVT_WAKE_WORD,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(100)
        );

        if (bits & EVT_WAKE_WORD) {
            set_app_state(APP_STATE_LISTENING);
            set_led_pattern(APP_STATE_LISTENING);
            update_activity();

            ESP_LOGI(TAG, "Voice input started");

            // Read audio from ReSpeaker
            if (usb_audio_read(audio_buffer, sizeof(audio_buffer), &bytes_read) == ESP_OK) {
                if (bytes_read > 0) {
                    set_app_state(APP_STATE_PROCESSING);
                    // TODO: Send to Home Assistant for STT/Intent processing
                }
            }

            set_app_state(APP_STATE_ACTIVE);
            set_led_pattern(APP_STATE_ACTIVE);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief Home Assistant communication task
 *
 * Handles sensor data upload and command reception.
 */
static void ha_task(void *pvParameters)
{
    ESP_LOGI(TAG, "HA task started (Core %d)", xPortGetCoreID());

    // Wait for WiFi
    xEventGroupWaitBits(s_app_event_group, EVT_WIFI_CONNECTED,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    // Connect to Home Assistant
    if (ha_client_init() == ESP_OK) {
        xEventGroupSetBits(s_app_event_group, EVT_HA_CONNECTED);
        ESP_LOGI(TAG, "Connected to Home Assistant");
    } else {
        ESP_LOGW(TAG, "Failed to connect to Home Assistant");
    }

    TickType_t last_sensor_update = 0;
    const TickType_t sensor_update_interval = pdMS_TO_TICKS(60000);  // 60s

    while (1) {
        // Process HA messages
        ha_client_process();

        // Periodic sensor data upload
        TickType_t now = xTaskGetTickCount();
        if ((now - last_sensor_update) >= sensor_update_interval) {
            last_sensor_update = now;

            // Get sensor data
            room_environment_t room_data;
            if (sensor_hub_get_data(&room_data) == ESP_OK) {
                ESP_LOGI(TAG, "Sensor update - CO2: %.0f ppm, PM2.5: %.1f, Temp: %.1f°C",
                         room_data.air.co2_ppm,
                         room_data.air.pm2_5,
                         room_data.air.temperature);

                // TODO: Send to Home Assistant
                // ha_client_update_sensor("sensor.console_co2", room_data.air.co2_ppm);
                // ha_client_update_sensor("sensor.console_pm25", room_data.air.pm2_5);
                // ha_client_update_sensor("sensor.console_temperature", room_data.air.temperature);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief Display management task
 *
 * Handles display power management and UI updates.
 */
static void display_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Display task started (Core %d)", xPortGetCoreID());

    // Wait for display hardware
    xEventGroupWaitBits(s_app_event_group, EVT_DISPLAY_READY,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    while (1) {
        uint32_t now = esp_timer_get_time() / 1000;

        // Check for display timeout
        if (s_display_on && (now - s_last_activity_ms) > DISPLAY_TIMEOUT_MS) {
            // Check if user is still present
            EventBits_t bits = xEventGroupGetBits(s_app_event_group);
            if (!(bits & EVT_USER_PRESENT)) {
                ESP_LOGI(TAG, "Display timeout - turning off");
                // TODO: Turn off display backlight
                s_display_on = false;
                set_app_state(APP_STATE_IDLE);
            }
        }

        // TODO: LVGL task handler
        // lv_task_handler();

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief System monitor task
 *
 * Monitors system health and logs statistics.
 */
static void monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Monitor task started");

    while (1) {
        // Log heap status
        ESP_LOGI(TAG, "Free heap: %lu bytes, Min free: %lu bytes",
                 esp_get_free_heap_size(),
                 esp_get_minimum_free_heap_size());

        // Log sensor status
        ESP_LOGI(TAG, "Sensors: SEN66=%s, SEN0395=%s, VL53L1X=%s, PAJ7620=%s, VEML7700=%s",
                 sensor_hub_is_sensor_online("sen66") ? "OK" : "FAIL",
                 sensor_hub_is_sensor_online("sen0395") ? "OK" : "FAIL",
                 sensor_hub_is_sensor_online("vl53l1x") ? "OK" : "FAIL",
                 sensor_hub_is_sensor_online("paj7620") ? "OK" : "FAIL",
                 sensor_hub_is_sensor_online("veml7700") ? "OK" : "FAIL");

        // Get current room data
        room_environment_t room;
        if (sensor_hub_get_data(&room) == ESP_OK) {
            ESP_LOGI(TAG, "Room: CO2=%.0f ppm, PM2.5=%.1f µg/m³, Temp=%.1f°C, Humidity=%.1f%%, Light=%lu lux",
                     room.air.co2_ppm, room.air.pm2_5,
                     room.air.temperature, room.air.humidity,
                     room.light.lux);
            ESP_LOGI(TAG, "Presence: %s, Distance=%d mm, AQI=%s",
                     room.presence.is_present ? "Yes" : "No",
                     room.presence.tof_distance_mm,
                     sensor_hub_aqi_to_string(room.air.aqi_level));
        }

        vTaskDelay(pdMS_TO_TICKS(30000));  // Every 30 seconds
    }
}

// =============================================================================
// Main Entry Point
// =============================================================================

void app_main(void)
{
    ESP_LOGI(TAG, "╔═══════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║     ESP32-P4 Smart Home Console - Starting Up...          ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════════════════════════╝");

    ESP_LOGI(TAG, "Hardware: ESP32-P4-Function-EV-Board v1.5");
    ESP_LOGI(TAG, "Display: 7\" MIPI-DSI (800x480)");
    ESP_LOGI(TAG, "Audio: ReSpeaker XVF3800 + ES8311 + DFRobot Amp");
    ESP_LOGI(TAG, "Sensors: SEN66, SEN0395, VL53L1X, PAJ7620U2, VEML7700");

    // Create synchronization primitives
    s_state_mutex = xSemaphoreCreateMutex();
    s_app_event_group = xEventGroupCreate();

    if (!s_state_mutex || !s_app_event_group) {
        ESP_LOGE(TAG, "Failed to create synchronization primitives");
        return;
    }

    set_app_state(APP_STATE_BOOTING);
    set_led_pattern(APP_STATE_BOOTING);

    // === Phase 1: Core Initialization ===
    ESP_LOGI(TAG, "--- Phase 1: Core Initialization ---");

    ESP_ERROR_CHECK(init_nvs());
    ESP_LOGI(TAG, "[OK] NVS Flash");

    ESP_ERROR_CHECK(init_gpio());
    ESP_LOGI(TAG, "[OK] GPIO");

    // === Phase 2: Hardware Initialization ===
    ESP_LOGI(TAG, "--- Phase 2: Hardware Initialization ---");

    if (init_display() != ESP_OK) {
        ESP_LOGW(TAG, "[WARN] Display initialization incomplete");
    } else {
        ESP_LOGI(TAG, "[OK] Display");
    }

    if (init_audio() != ESP_OK) {
        ESP_LOGW(TAG, "[WARN] Audio initialization incomplete");
    } else {
        ESP_LOGI(TAG, "[OK] Audio");
    }

    if (init_sensors() != ESP_OK) {
        ESP_LOGW(TAG, "[WARN] Sensor initialization incomplete");
    } else {
        ESP_LOGI(TAG, "[OK] Sensor Hub");
    }

    // === Phase 3: Network Initialization ===
    ESP_LOGI(TAG, "--- Phase 3: Network Initialization ---");

    if (init_network() != ESP_OK) {
        ESP_LOGW(TAG, "[WARN] Network initialization incomplete");
    } else {
        ESP_LOGI(TAG, "[OK] Network");
    }

    // === Phase 4: Start Application Tasks ===
    ESP_LOGI(TAG, "--- Phase 4: Starting Tasks ---");

    // Core 1: UI and Display (higher priority)
    xTaskCreatePinnedToCore(display_task, "display", 8192, NULL, 6, NULL, 1);

    // Core 0: Audio processing
    xTaskCreatePinnedToCore(audio_task, "audio", 8192, NULL, 5, NULL, 0);

    // Core 0: Home Assistant communication
    xTaskCreatePinnedToCore(ha_task, "ha_client", 4096, NULL, 4, NULL, 0);

    // Core 0: System monitor (low priority)
    xTaskCreatePinnedToCore(monitor_task, "monitor", 4096, NULL, 2, NULL, 0);

    // === System Ready ===
    set_app_state(APP_STATE_IDLE);
    set_led_pattern(APP_STATE_IDLE);
    update_activity();

    ESP_LOGI(TAG, "╔═══════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║           System Ready - All Tasks Running                ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════════════════════════╝");
}
