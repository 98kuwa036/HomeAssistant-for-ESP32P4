/**
 * Omni-P4: ESP32-P4 Smart Home Console - Main Application
 *
 * Hardware Configuration:
 *   - 7" MIPI-DSI Display (800x480) - Bowers & Wilkins Formation Wedge style
 *   - Dual I2S Audio:
 *     - I2S0 TX: ES9039Q2M DAC → Peerless Amp → Dayton Passive Radiator
 *     - I2S1 RX: ReSpeaker XVF3800 (4-mic array with DSP)
 *   - DFR0759 I2C Hub (6 ports) → Multiple Sensors:
 *     - SCD41: CO2, Temperature, Humidity
 *     - SGP40: VOC Index
 *     - ENS160: eCO2, TVOC, AQI
 *     - BMP388: Barometric pressure, Altitude
 *     - SHT40: High-precision Temperature/Humidity
 *     - SEN0428: PM1.0, PM2.5, PM10
 *   - UART Devices:
 *     - SEN0395: mmWave radar → Human presence (UART1)
 *     - SEN0540: Offline voice recognition (UART2)
 *
 * Software Architecture:
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │                     Application Layer                       │
 *   │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌───────────────┐  │
 *   │  │  Audio  │  │   UI    │  │   HA    │  │  Sensor Hub   │  │
 *   │  │  Task   │  │  Task   │  │  Task   │  │  (3 tasks)    │  │
 *   │  └────┬────┘  └────┬────┘  └────┬────┘  └───────┬───────┘  │
 *   └───────┼────────────┼───────────┼────────────────┼──────────┘
 *           │            │           │                │
 *   ┌───────┴────────────┴───────────┴────────────────┴──────────┐
 *   │                    Hardware Abstraction                     │
 *   │  ES9039 DAC │ XVF3800 │ MIPI-DSI │ I2C Sensors │ UART      │
 *   └─────────────────────────────────────────────────────────────┘
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

#include "wifi_manager.h"
#include "ha_client.h"
#include "sensor_hub.h"
#include "drivers/es9039.h"
#include "drivers/xvf3800.h"

static const char *TAG = "OMNI-P4";

// =============================================================================
// Hardware Pin Definitions
// =============================================================================

// Status LED
#define LED_STATUS_PIN          GPIO_NUM_47

// Boot Button
#define BUTTON_BOOT_PIN         GPIO_NUM_0

// I2S0 Pins (ES9039Q2M DAC - TX)
#define I2S0_MCLK_PIN           GPIO_NUM_13
#define I2S0_BCLK_PIN           GPIO_NUM_12
#define I2S0_WS_PIN             GPIO_NUM_10
#define I2S0_DOUT_PIN           GPIO_NUM_9

// I2S1 Pins (XVF3800 ReSpeaker - RX)
#define I2S1_BCLK_PIN           GPIO_NUM_45
#define I2S1_WS_PIN             GPIO_NUM_46
#define I2S1_DIN_PIN            GPIO_NUM_47

// I2C Pins (DFR0759 Hub → Multiple Sensors)
#define I2C_SDA_PIN             GPIO_NUM_7
#define I2C_SCL_PIN             GPIO_NUM_8

// UART1 Pins (SEN0395 mmWave Radar)
#define UART1_TX_PIN            GPIO_NUM_17
#define UART1_RX_PIN            GPIO_NUM_18

// UART2 Pins (SEN0540 Voice Recognition)
#define UART2_TX_PIN            GPIO_NUM_4
#define UART2_RX_PIN            GPIO_NUM_5

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
#define EVT_AUDIO_DAC_READY     BIT2
#define EVT_AUDIO_MIC_READY     BIT3
#define EVT_DISPLAY_READY       BIT4
#define EVT_SENSORS_READY       BIT5
#define EVT_WAKE_WORD           BIT6
#define EVT_USER_PRESENT        BIT7
#define EVT_VOICE_COMMAND       BIT8

// =============================================================================
// Display State
// =============================================================================

static bool s_display_on = false;
static uint32_t s_last_activity_ms = 0;
#define DISPLAY_TIMEOUT_MS      (60 * 1000)  // 60 seconds

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
            gpio_set_level(LED_STATUS_PIN, 1);
            break;
        case APP_STATE_PROCESSING:
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
 * @brief Presence change callback (SEN0395 mmWave)
 */
static void on_presence_changed(bool is_present, void *user_data)
{
    ESP_LOGI(TAG, "Presence: %s", is_present ? "DETECTED" : "NONE");

    if (is_present) {
        xEventGroupSetBits(s_app_event_group, EVT_USER_PRESENT);
        set_app_state(APP_STATE_ACTIVE);
        update_activity();

        // Wake display when user detected
        if (!s_display_on) {
            ESP_LOGI(TAG, "User detected - waking display");
            s_display_on = true;
        }
    } else {
        xEventGroupClearBits(s_app_event_group, EVT_USER_PRESENT);

        // Allow display to turn off after timeout
        if (get_app_state() == APP_STATE_ACTIVE) {
            set_app_state(APP_STATE_IDLE);
        }
    }
}

/**
 * @brief Voice command callback (SEN0540)
 */
static void on_voice_command(uint8_t cmd_id, const char *keyword, void *user_data)
{
    ESP_LOGI(TAG, "Voice command: ID=%d, Keyword='%s'", cmd_id, keyword);
    update_activity();

    xEventGroupSetBits(s_app_event_group, EVT_VOICE_COMMAND);

    // Handle built-in commands
    switch (cmd_id) {
        case 0:  // Wake word
            ESP_LOGI(TAG, "Wake word detected");
            xEventGroupSetBits(s_app_event_group, EVT_WAKE_WORD);
            break;
        case 1:  // Light on
            ESP_LOGI(TAG, "Command: Light On");
            // TODO: Send to Home Assistant
            break;
        case 2:  // Light off
            ESP_LOGI(TAG, "Command: Light Off");
            // TODO: Send to Home Assistant
            break;
        case 3:  // Volume up
            ESP_LOGI(TAG, "Command: Volume Up");
            break;
        case 4:  // Volume down
            ESP_LOGI(TAG, "Command: Volume Down");
            break;
        case 5:  // Play
            ESP_LOGI(TAG, "Command: Play");
            break;
        case 6:  // Pause
            ESP_LOGI(TAG, "Command: Pause");
            break;
        default:
            ESP_LOGI(TAG, "Custom command: %d", cmd_id);
            break;
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

    return ESP_OK;
}

static esp_err_t init_audio(void)
{
    esp_err_t ret;

    // Initialize ES9039 DAC (I2S0 TX)
    es9039_config_t dac_config = {
        .mclk_pin = I2S0_MCLK_PIN,
        .bclk_pin = I2S0_BCLK_PIN,
        .ws_pin = I2S0_WS_PIN,
        .dout_pin = I2S0_DOUT_PIN,
        .sample_rate = ES9039_SAMPLE_RATE_48000,
        .bits_per_sample = ES9039_BITS_16,
        .use_i2c_control = false,
        .i2c_sda_pin = -1,
        .i2c_scl_pin = -1,
    };

    ret = es9039_init(&dac_config);
    if (ret == ESP_OK) {
        xEventGroupSetBits(s_app_event_group, EVT_AUDIO_DAC_READY);
        ESP_LOGI(TAG, "ES9039 DAC initialized (I2S0 TX, 48kHz)");
    } else {
        ESP_LOGW(TAG, "ES9039 DAC init failed: %s", esp_err_to_name(ret));
    }

    // Initialize XVF3800 Voice Processor (I2S1 RX)
    xvf3800_config_t mic_config = {
        .bclk_pin = I2S1_BCLK_PIN,
        .ws_pin = I2S1_WS_PIN,
        .din_pin = I2S1_DIN_PIN,
        .sample_rate = 16000,
        .bits_per_sample = 16,
    };

    ret = xvf3800_init(&mic_config);
    if (ret == ESP_OK) {
        ret = xvf3800_start();
        if (ret == ESP_OK) {
            xEventGroupSetBits(s_app_event_group, EVT_AUDIO_MIC_READY);
            ESP_LOGI(TAG, "XVF3800 initialized (I2S1 RX, 16kHz)");
        } else {
            ESP_LOGW(TAG, "XVF3800 start failed: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGW(TAG, "XVF3800 init failed: %s", esp_err_to_name(ret));
    }

    return ESP_OK;
}

static esp_err_t init_sensors(void)
{
    sensor_hub_config_t config = {
        .i2c_sda_pin = I2C_SDA_PIN,
        .i2c_scl_pin = I2C_SCL_PIN,
        .i2c_freq_hz = 100000,
        .uart1_tx_pin = UART1_TX_PIN,
        .uart1_rx_pin = UART1_RX_PIN,
        .uart2_tx_pin = UART2_TX_PIN,
        .uart2_rx_pin = UART2_RX_PIN,
        .task_core = 0,
        .task_priority = 5,
        .altitude_m = 0,
    };

    esp_err_t ret = sensor_hub_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Sensor hub init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register callbacks
    sensor_hub_register_presence_callback(on_presence_changed, NULL);
    sensor_hub_register_voice_callback(on_voice_command, NULL);

    xEventGroupSetBits(s_app_event_group, EVT_SENSORS_READY);
    ESP_LOGI(TAG, "Sensor hub initialized with callbacks");

    return ESP_OK;
}

static esp_err_t init_display(void)
{
    // TODO: Initialize 7" MIPI-DSI display (800x480)
    // This will be implemented with LVGL

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
 * Handles voice input from XVF3800 and audio output via ES9039.
 */
static void audio_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Audio task started (Core %d)", xPortGetCoreID());

    // Wait for audio hardware
    xEventGroupWaitBits(s_app_event_group,
                        EVT_AUDIO_DAC_READY | EVT_AUDIO_MIC_READY,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    int16_t audio_buffer[512];
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

            // Read audio from XVF3800
            if (xvf3800_read(audio_buffer, sizeof(audio_buffer), &bytes_read, 1000) == ESP_OK) {
                if (bytes_read > 0) {
                    set_app_state(APP_STATE_PROCESSING);
                    // TODO: Process audio or send to Home Assistant for STT
                    ESP_LOGI(TAG, "Captured %d bytes of audio", bytes_read);
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
                ESP_LOGI(TAG, "Sensor update:");
                ESP_LOGI(TAG, "  CO2: %u ppm, VOC: %ld, AQI: %s",
                         room_data.co2.co2_ppm,
                         room_data.voc.voc_index,
                         sensor_hub_aqi_to_string(room_data.ens_aqi.aqi));
                ESP_LOGI(TAG, "  PM2.5: %u µg/m³, Pressure: %.1f hPa",
                         room_data.pm.pm2_5,
                         room_data.baro.pressure);
                ESP_LOGI(TAG, "  Temp: %.1f°C, Humidity: %.1f%%",
                         room_data.precision.temperature,
                         room_data.precision.humidity);

                // TODO: Send to Home Assistant
                // ha_client_update_sensors(&room_data);
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
        ESP_LOGI(TAG, "I2C Sensors: SCD41=%s, SGP40=%s, ENS160=%s, BMP388=%s, SHT40=%s, SEN0428=%s",
                 sensor_hub_is_sensor_online("scd41") ? "OK" : "-",
                 sensor_hub_is_sensor_online("sgp40") ? "OK" : "-",
                 sensor_hub_is_sensor_online("ens160") ? "OK" : "-",
                 sensor_hub_is_sensor_online("bmp388") ? "OK" : "-",
                 sensor_hub_is_sensor_online("sht40") ? "OK" : "-",
                 sensor_hub_is_sensor_online("sen0428") ? "OK" : "-");

        ESP_LOGI(TAG, "UART Devices: SEN0395=%s, SEN0540=%s",
                 sensor_hub_is_sensor_online("sen0395") ? "OK" : "-",
                 sensor_hub_is_sensor_online("sen0540") ? "OK" : "-");

        ESP_LOGI(TAG, "Audio: DAC=%s, MIC=%s",
                 es9039_is_initialized() ? "OK" : "-",
                 xvf3800_is_initialized() ? "OK" : "-");

        // Get current room data
        room_environment_t room;
        if (sensor_hub_get_data(&room) == ESP_OK) {
            ESP_LOGI(TAG, "Environment:");
            ESP_LOGI(TAG, "  CO2=%u ppm, eCO2=%u ppm, TVOC=%u ppb, VOC=%ld",
                     room.co2.co2_ppm, room.ens_aqi.eco2,
                     room.ens_aqi.tvoc, room.voc.voc_index);
            ESP_LOGI(TAG, "  PM1.0=%u, PM2.5=%u, PM10=%u µg/m³",
                     room.pm.pm1_0, room.pm.pm2_5, room.pm.pm10);
            ESP_LOGI(TAG, "  Temp=%.1f°C (±0.2), Humidity=%.1f%% (±1.8)",
                     room.precision.temperature, room.precision.humidity);
            ESP_LOGI(TAG, "  Pressure=%.1f hPa, Altitude=%.1f m",
                     room.baro.pressure, room.baro.altitude);
            ESP_LOGI(TAG, "  AQI=%s, Presence=%s",
                     sensor_hub_aqi_to_string(room.ens_aqi.aqi),
                     room.presence.is_present ? "Yes" : "No");
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
    ESP_LOGI(TAG, "║       Omni-P4: ESP32-P4 Smart Home Console                ║");
    ESP_LOGI(TAG, "║       Formation Wedge Style with Home Assistant           ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════════════════════════╝");

    ESP_LOGI(TAG, "Hardware Configuration:");
    ESP_LOGI(TAG, "  Display: 7\" MIPI-DSI (800x480)");
    ESP_LOGI(TAG, "  Audio Out: ES9039Q2M DAC → Peerless Amp (I2S0 TX)");
    ESP_LOGI(TAG, "  Audio In: ReSpeaker XVF3800 4-mic array (I2S1 RX)");
    ESP_LOGI(TAG, "  I2C Sensors: SCD41, SGP40, ENS160, BMP388, SHT40, SEN0428");
    ESP_LOGI(TAG, "  UART Devices: SEN0395 (radar), SEN0540 (voice)");

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
        ESP_LOGI(TAG, "[OK] Audio (ES9039 + XVF3800)");
    }

    if (init_sensors() != ESP_OK) {
        ESP_LOGW(TAG, "[WARN] Sensor initialization incomplete");
    } else {
        ESP_LOGI(TAG, "[OK] Sensor Hub (8 sensors)");
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
