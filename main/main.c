/**
 * @file main.c
 * @brief Omni-P4 Smart Speaker - Main Application Entry
 *
 * This is the main entry point for the Omni-P4 firmware.
 * It initializes all subsystems and starts FreeRTOS tasks.
 *
 * Architecture:
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │                      app_main()                              │
 *   ├─────────────────────────────────────────────────────────────┤
 *   │  1. System Init (NVS, Event Loop, Network)                  │
 *   │  2. Audio Pipeline Init (I2S0: DAC, I2S1: Mic)             │
 *   │  3. Display Init (MIPI-DSI + LVGL)                          │
 *   │  4. Sensor Hub Init (I2C + UART sensors)                    │
 *   │  5. LED Effect Init (WS2812B via RMT)                       │
 *   │  6. Start FreeRTOS Tasks                                    │
 *   └─────────────────────────────────────────────────────────────┘
 *
 * Task Priority Layout:
 *   Priority 5: Audio Task (highest - real-time audio)
 *   Priority 4: Display Task (LVGL rendering)
 *   Priority 3: LED Effect Task
 *   Priority 2: Sensor Task
 *   Priority 1: MQTT/Network Task
 *
 * @author Omni-P4 Project
 * @date 2024
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_psram.h"
#include "esp_heap_caps.h"

// Component headers
#include "sensor_hub.h"
#include "audio_pipeline.h"
#include "led_effect.h"
#include "display_manager.h"
#include "command_cache.h"
#include "remo_client.h"

static const char *TAG = "omni_p4";

// ============================================================================
// FreeRTOS Task Handles
// ============================================================================

static TaskHandle_t s_audio_task_handle = NULL;
static TaskHandle_t s_display_task_handle = NULL;
static TaskHandle_t s_sensor_task_handle = NULL;
static TaskHandle_t s_led_task_handle = NULL;
static TaskHandle_t s_network_task_handle = NULL;

// ============================================================================
// Event Groups
// ============================================================================

static EventGroupHandle_t s_system_event_group;

#define SYSTEM_INIT_COMPLETE_BIT    BIT0
#define WIFI_CONNECTED_BIT          BIT1
#define MQTT_CONNECTED_BIT          BIT2
#define AUDIO_READY_BIT             BIT3
#define DISPLAY_READY_BIT           BIT4
#define SENSORS_READY_BIT           BIT5

// ============================================================================
// System State
// ============================================================================

typedef struct {
    bool audio_enabled;
    bool display_enabled;
    bool sensors_enabled;
    bool led_enabled;
    bool wifi_connected;
    bool mqtt_connected;
    uint32_t uptime_seconds;
    uint32_t free_heap;
    uint32_t free_psram;
} system_state_t;

static system_state_t s_system_state = {0};
static SemaphoreHandle_t s_state_mutex = NULL;

// ============================================================================
// Forward Declarations
// ============================================================================

static void audio_task(void *pvParameters);
static void display_task(void *pvParameters);
static void sensor_task(void *pvParameters);
static void led_task(void *pvParameters);
static void network_task(void *pvParameters);
static void system_monitor_task(void *pvParameters);

// ============================================================================
// System Initialization
// ============================================================================

/**
 * @brief Initialize NVS (Non-Volatile Storage)
 */
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

/**
 * @brief Initialize PSRAM and print memory info
 */
static void init_memory(void)
{
    // Check PSRAM
    size_t psram_size = esp_psram_get_size();
    if (psram_size > 0) {
        ESP_LOGI(TAG, "PSRAM initialized: %zu bytes (%.1f MB)",
                 psram_size, psram_size / 1024.0 / 1024.0);
    } else {
        ESP_LOGW(TAG, "No PSRAM detected!");
    }

    // Print heap info
    ESP_LOGI(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
    ESP_LOGI(TAG, "Free PSRAM: %zu bytes",
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "Minimum free heap: %lu bytes",
             (unsigned long)esp_get_minimum_free_heap_size());
}

/**
 * @brief Print system information
 */
static void print_system_info(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "╔════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║           Omni-P4 Smart Speaker Firmware               ║");
    ESP_LOGI(TAG, "╠════════════════════════════════════════════════════════╣");
    ESP_LOGI(TAG, "║ Chip: ESP32-P4 (%d cores @ %d MHz)                     ║",
             chip_info.cores, CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
    ESP_LOGI(TAG, "║ ESP-IDF Version: %s                          ║", esp_get_idf_version());
    ESP_LOGI(TAG, "║ Board: %s              ║", CONFIG_OMNI_P4_BOARD_NAME);
    ESP_LOGI(TAG, "╚════════════════════════════════════════════════════════╝");
}

// ============================================================================
// FreeRTOS Tasks
// ============================================================================

/**
 * @brief Audio processing task (highest priority)
 *
 * Handles:
 * - I2S data streaming (DAC output)
 * - Mic input processing (from ReSpeaker)
 * - Voice activity detection
 * - Audio buffer management
 */
static void audio_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Audio task started");

#if CONFIG_OMNI_P4_AUDIO_ENABLED
    // Wait for audio pipeline to be ready
    audio_pipeline_wait_ready(portMAX_DELAY);

    xEventGroupSetBits(s_system_event_group, AUDIO_READY_BIT);
    ESP_LOGI(TAG, "Audio pipeline ready");

    while (1) {
        // Process audio data
        audio_pipeline_process();

        // Check for voice activity
        if (audio_pipeline_voice_detected()) {
            // Notify LED task for visual feedback
            if (s_led_task_handle) {
                xTaskNotify(s_led_task_handle, LED_NOTIFY_VOICE_ACTIVE, eSetBits);
            }
        }

        // Small delay to prevent CPU hogging
        vTaskDelay(pdMS_TO_TICKS(5));
    }
#else
    ESP_LOGW(TAG, "Audio disabled in config");
    vTaskDelete(NULL);
#endif
}

/**
 * @brief Display task (LVGL rendering)
 *
 * Handles:
 * - LVGL timer processing
 * - UI updates
 * - Touch input
 */
static void display_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Display task started");

#if CONFIG_OMNI_P4_DISPLAY_ENABLED
    // Initialize display
    esp_err_t ret = display_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display init failed: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }

    xEventGroupSetBits(s_system_event_group, DISPLAY_READY_BIT);
    ESP_LOGI(TAG, "Display ready");

    // LVGL main loop
    while (1) {
        // Lock LVGL mutex and call timer handler
        display_manager_lock(-1);
        uint32_t delay_ms = display_manager_timer_handler();
        display_manager_unlock();

        // Wait for next LVGL tick
        if (delay_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        } else {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
#else
    ESP_LOGW(TAG, "Display disabled in config");
    vTaskDelete(NULL);
#endif
}

/**
 * @brief Sensor reading task
 *
 * Handles:
 * - Periodic sensor readings (I2C sensors)
 * - UART sensor data (LD2410, SEN0540)
 * - Data aggregation and JSON formatting
 */
static void sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Sensor task started");

#if CONFIG_OMNI_P4_SENSORS_ENABLED
    // Initialize sensor hub
    esp_err_t ret = sensor_hub_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Sensor hub init failed: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }

    xEventGroupSetBits(s_system_event_group, SENSORS_READY_BIT);
    ESP_LOGI(TAG, "Sensor hub ready");

    sensor_data_t sensor_data;
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        // Read all sensors
        ret = sensor_hub_read_all(&sensor_data);
        if (ret == ESP_OK) {
            // Log sensor data (debug)
            ESP_LOGD(TAG, "Temp: %.1f°C, Hum: %.1f%%, CO2: %d ppm",
                     sensor_data.temperature,
                     sensor_data.humidity,
                     sensor_data.co2);

            // Update display if ready
            EventBits_t bits = xEventGroupGetBits(s_system_event_group);
            if (bits & DISPLAY_READY_BIT) {
                display_manager_update_sensors(&sensor_data);
            }

            // Publish to MQTT if connected
            if (bits & MQTT_CONNECTED_BIT) {
                // sensor_hub_publish_mqtt(&sensor_data);
            }
        }

        // Wait for next reading interval
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CONFIG_SENSOR_READ_INTERVAL_MS));
    }
#else
    ESP_LOGW(TAG, "Sensors disabled in config");
    vTaskDelete(NULL);
#endif
}

/**
 * @brief LED effect task (WS2812B ring)
 *
 * Handles:
 * - Breathing effect (idle state)
 * - Voice activity indication
 * - Status indication (WiFi, errors)
 */
static void led_task(void *pvParameters)
{
    ESP_LOGI(TAG, "LED task started");

#if CONFIG_OMNI_P4_LED_ENABLED
    // Initialize LED strip
    esp_err_t ret = led_effect_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LED init failed: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "LED strip ready");

    // Set initial effect (breathing)
    led_effect_set_mode(LED_MODE_BREATHING);

    uint32_t notify_value;
    while (1) {
        // Check for notifications from other tasks
        if (xTaskNotifyWait(0, ULONG_MAX, &notify_value, pdMS_TO_TICKS(20))) {
            if (notify_value & LED_NOTIFY_VOICE_ACTIVE) {
                led_effect_set_mode(LED_MODE_LISTENING);
            } else if (notify_value & LED_NOTIFY_VOICE_PROCESSING) {
                led_effect_set_mode(LED_MODE_THINKING);
            } else if (notify_value & LED_NOTIFY_VOICE_DONE) {
                led_effect_set_mode(LED_MODE_BREATHING);
            } else if (notify_value & LED_NOTIFY_ERROR) {
                led_effect_flash_error();
            }
        }

        // Update LED animation
        led_effect_update();
    }
#else
    ESP_LOGW(TAG, "LEDs disabled in config");
    vTaskDelete(NULL);
#endif
}

/**
 * @brief Network task (WiFi, MQTT)
 *
 * Handles:
 * - WiFi connection management
 * - MQTT client
 * - Home Assistant discovery
 */
static void network_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Network task started");

    // TODO: Implement WiFi and MQTT initialization
    // For now, just mark as placeholder

    // Initialize Nature Remo client
    // This will mount SPIFFS and attempt mDNS discovery
    esp_err_t remo_ret = remo_client_init();
    if (remo_ret == ESP_OK) {
        ESP_LOGI(TAG, "Remo client initialized, available: %s",
                 remo_client_is_available() ? "yes" : "no");
    } else {
        ESP_LOGW(TAG, "Remo client init failed: %s", esp_err_to_name(remo_ret));
    }

    while (1) {
        // Update system state
        if (s_state_mutex && xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100))) {
            s_system_state.free_heap = esp_get_free_heap_size();
            s_system_state.free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
            s_system_state.uptime_seconds = esp_timer_get_time() / 1000000;
            xSemaphoreGive(s_state_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief System monitor task
 *
 * Handles:
 * - Memory monitoring
 * - Watchdog feeding
 * - Status logging
 */
static void system_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "System monitor started");

    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        // Log system status every 30 seconds
        ESP_LOGI(TAG, "System: Heap=%lu, PSRAM=%zu, Uptime=%lus",
                 (unsigned long)esp_get_free_heap_size(),
                 heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                 (unsigned long)(esp_timer_get_time() / 1000000));

        // Check for low memory
        if (esp_get_free_heap_size() < 50000) {
            ESP_LOGW(TAG, "Low heap warning!");
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(30000));
    }
}

// ============================================================================
// Application Entry Point
// ============================================================================

void app_main(void)
{
    // Print boot banner
    print_system_info();

    // Initialize memory and NVS
    init_memory();
    ESP_ERROR_CHECK(init_nvs());

    // Create event group and mutex
    s_system_event_group = xEventGroupCreate();
    s_state_mutex = xSemaphoreCreateMutex();
    assert(s_system_event_group != NULL);
    assert(s_state_mutex != NULL);

    // Initialize default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize command cache for voice commands
    cmd_cache_init();
    ESP_LOGI(TAG, "Command cache initialized with %d commands", cmd_cache_get_count());

    // ========================================================================
    // Initialize Audio Pipeline (before starting tasks)
    // ========================================================================
#if CONFIG_OMNI_P4_AUDIO_ENABLED
    ESP_LOGI(TAG, "Initializing audio pipeline...");
    esp_err_t ret = audio_pipeline_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Audio pipeline init failed: %s", esp_err_to_name(ret));
        s_system_state.audio_enabled = false;
    } else {
        s_system_state.audio_enabled = true;
    }
#endif

    // ========================================================================
    // Create FreeRTOS Tasks
    // ========================================================================

    ESP_LOGI(TAG, "Creating FreeRTOS tasks...");

    // Audio task (highest priority, pinned to core 1)
    xTaskCreatePinnedToCore(
        audio_task,
        "audio",
        8192,
        NULL,
        5,  // Highest priority
        &s_audio_task_handle,
        1   // Core 1
    );

    // Display task (high priority, pinned to core 0)
    xTaskCreatePinnedToCore(
        display_task,
        "display",
        8192,
        NULL,
        4,
        &s_display_task_handle,
        0   // Core 0
    );

    // LED effect task
    xTaskCreatePinnedToCore(
        led_task,
        "led",
        4096,
        NULL,
        3,
        &s_led_task_handle,
        0
    );

    // Sensor task
    xTaskCreatePinnedToCore(
        sensor_task,
        "sensor",
        6144,
        NULL,
        2,
        &s_sensor_task_handle,
        0
    );

    // Network task
    xTaskCreatePinnedToCore(
        network_task,
        "network",
        6144,
        NULL,
        1,
        &s_network_task_handle,
        0
    );

    // System monitor (lowest priority)
    xTaskCreate(
        system_monitor_task,
        "sysmon",
        2048,
        NULL,
        0,  // Lowest priority
        NULL
    );

    // Mark system as initialized
    xEventGroupSetBits(s_system_event_group, SYSTEM_INIT_COMPLETE_BIT);

    ESP_LOGI(TAG, "╔════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║        Omni-P4 initialization complete!                ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════════════════════════╝");

    // app_main can return, scheduler keeps running
}
