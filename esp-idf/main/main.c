/**
 * ESP32-P4 Smart Speaker with ReSpeaker USB Mic Array
 *
 * Hardware Configuration:
 *   - ReSpeaker USB Mic Array (XVF3800) → USB Host
 *   - ES8311 DAC → PAM8403 → Peerless Speakers
 *   - SCD40 (CO2/Temp/Humidity) → I2C
 *   - SPS30 (PM Sensor) → I2C
 *   - OV5640 Camera → MIPI CSI (future)
 *
 * Architecture:
 *   USB Audio (ReSpeaker) → ESP32-P4 → WiFi → Home Assistant
 *                                    ↓
 *                              I2S → ES8311 → PAM8403 → Speakers
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "usb_audio.h"
#include "i2s_output.h"
#include "wifi_manager.h"
#include "ha_client.h"

static const char *TAG = "MAIN";

// GPIO Definitions (ESP32-P4-Function-EV-Board)
#define LED_STATUS_PIN      GPIO_NUM_47
#define BUTTON_BOOT_PIN     GPIO_NUM_0

// I2S Pins (ES8311)
#define I2S_MCLK_PIN        GPIO_NUM_13
#define I2S_BCLK_PIN        GPIO_NUM_12
#define I2S_WS_PIN          GPIO_NUM_10
#define I2S_DOUT_PIN        GPIO_NUM_9
#define I2S_DIN_PIN         GPIO_NUM_11

// I2C Pins (Sensors)
#define I2C_SDA_PIN         GPIO_NUM_7
#define I2C_SCL_PIN         GPIO_NUM_8

// Application state
typedef enum {
    STATE_IDLE,
    STATE_LISTENING,
    STATE_PROCESSING,
    STATE_SPEAKING,
    STATE_ERROR
} app_state_t;

static app_state_t current_state = STATE_IDLE;
static EventGroupHandle_t app_event_group;

// Event bits
#define WIFI_CONNECTED_BIT      BIT0
#define HA_CONNECTED_BIT        BIT1
#define USB_AUDIO_READY_BIT     BIT2
#define WAKE_WORD_DETECTED_BIT  BIT3

/**
 * LED status indication
 */
static void set_led_state(app_state_t state)
{
    // TODO: Implement WS2812 LED control
    // For now, just use simple GPIO
    switch (state) {
        case STATE_IDLE:
            gpio_set_level(LED_STATUS_PIN, 0);
            break;
        case STATE_LISTENING:
        case STATE_PROCESSING:
        case STATE_SPEAKING:
            gpio_set_level(LED_STATUS_PIN, 1);
            break;
        case STATE_ERROR:
            // Blink pattern for error
            gpio_set_level(LED_STATUS_PIN, 1);
            break;
    }
}

/**
 * Button interrupt handler
 */
static void IRAM_ATTR button_isr_handler(void *arg)
{
    // Toggle listening state
    if (current_state == STATE_IDLE) {
        xEventGroupSetBits(app_event_group, WAKE_WORD_DETECTED_BIT);
    }
}

/**
 * Initialize GPIO
 */
static esp_err_t init_gpio(void)
{
    // Configure LED
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_STATUS_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&led_conf));

    // Configure Button with interrupt
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BUTTON_BOOT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&btn_conf));

    // Install ISR service
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_BOOT_PIN, button_isr_handler, NULL);

    return ESP_OK;
}

/**
 * Audio processing task
 * Handles USB microphone input and I2S speaker output
 */
static void audio_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Audio task started");

    // Wait for USB audio to be ready
    xEventGroupWaitBits(app_event_group, USB_AUDIO_READY_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    uint8_t audio_buffer[1024];
    size_t bytes_read;

    while (1) {
        EventBits_t bits = xEventGroupWaitBits(
            app_event_group,
            WAKE_WORD_DETECTED_BIT,
            pdTRUE,  // Clear on exit
            pdFALSE,
            pdMS_TO_TICKS(100)
        );

        if (bits & WAKE_WORD_DETECTED_BIT) {
            current_state = STATE_LISTENING;
            set_led_state(current_state);
            ESP_LOGI(TAG, "Listening started...");

            // Read from USB microphone
            if (usb_audio_read(audio_buffer, sizeof(audio_buffer), &bytes_read) == ESP_OK) {
                if (bytes_read > 0) {
                    // TODO: Send to Home Assistant for STT processing
                    // ha_client_send_audio(audio_buffer, bytes_read);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * Home Assistant communication task
 */
static void ha_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Home Assistant task started");

    // Wait for WiFi connection
    xEventGroupWaitBits(app_event_group, WIFI_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    // Connect to Home Assistant
    if (ha_client_init() == ESP_OK) {
        xEventGroupSetBits(app_event_group, HA_CONNECTED_BIT);
        ESP_LOGI(TAG, "Connected to Home Assistant");
    }

    while (1) {
        // Handle HA communication
        ha_client_process();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * Sensor reading task
 */
static void sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Sensor task started");

    while (1) {
        // TODO: Read SCD40 (CO2, Temperature, Humidity)
        // TODO: Read SPS30 (PM1.0, PM2.5, PM4.0, PM10)
        // TODO: Send to Home Assistant

        vTaskDelay(pdMS_TO_TICKS(60000));  // Read every 60 seconds
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-P4 Smart Speaker Starting...");
    ESP_LOGI(TAG, "Hardware: ReSpeaker USB + PAM8403 + SCD40 + SPS30");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Create event group
    app_event_group = xEventGroupCreate();

    // Initialize GPIO
    ESP_ERROR_CHECK(init_gpio());
    ESP_LOGI(TAG, "GPIO initialized");

    // Initialize WiFi
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_LOGI(TAG, "WiFi manager initialized");

    // Initialize USB Audio (ReSpeaker)
    usb_audio_config_t usb_config = {
        .sample_rate = 16000,
        .channels = 1,
        .bits_per_sample = 16,
    };
    if (usb_audio_init(&usb_config) == ESP_OK) {
        xEventGroupSetBits(app_event_group, USB_AUDIO_READY_BIT);
        ESP_LOGI(TAG, "USB Audio (ReSpeaker) initialized");
    } else {
        ESP_LOGE(TAG, "Failed to initialize USB Audio");
    }

    // Initialize I2S Output (ES8311 → PAM8403)
    i2s_output_config_t i2s_config = {
        .mclk_pin = I2S_MCLK_PIN,
        .bclk_pin = I2S_BCLK_PIN,
        .ws_pin = I2S_WS_PIN,
        .dout_pin = I2S_DOUT_PIN,
        .sample_rate = 16000,
        .bits_per_sample = 16,
    };
    ESP_ERROR_CHECK(i2s_output_init(&i2s_config));
    ESP_LOGI(TAG, "I2S Output (ES8311) initialized");

    // Start tasks
    xTaskCreate(audio_task, "audio_task", 8192, NULL, 5, NULL);
    xTaskCreate(ha_task, "ha_task", 4096, NULL, 4, NULL);
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "All tasks started. System ready.");
    set_led_state(STATE_IDLE);
}
