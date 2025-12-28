/**
 * Omni-P4 v4.1 (B&W Trace) - ESP32-P4 Smart Speaker
 *
 * Hardware Configuration (2025-01 Final):
 *   - Microphone: ReSpeaker USB Mic Array v2.0 → USB Host (UAC)
 *   - DAC: ES9039Q2M → I2S0 (Output only)
 *   - Display: 7" MIPI-DSI LCD (1024 x 600)
 *   - Sensors (8 modules via Sensor Hub):
 *       I2C: SCD41, SGP40, ENS160, BMP388, SHT40, SEN0428
 *       UART1: SEN0395 (mmWave Radar)
 *       UART2: SEN0540 (Formaldehyde)
 *
 * Audio Architecture (AEC Enabled):
 *   ESP32-P4 ──(USB)──► ReSpeaker v2.0 ──(3.5mm)──► External Amp ──► Speakers
 *                              │
 *                              └── USB Audio OUT (for AEC reference)
 *
 *   ESP32-P4 ──(I2S0)──► ES9039Q2M DAC ──► Line Out (Alternative)
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
#include "driver/i2c.h"
#include "driver/uart.h"

#include "usb_audio.h"
#include "i2s_output.h"
#include "wifi_manager.h"
#include "ha_client.h"

static const char *TAG = "OMNI_P4";

// =============================================================================
// Hardware Pin Definitions - Omni-P4 v4.1 (B&W Trace)
// =============================================================================

// Status LED
#define LED_STATUS_PIN          GPIO_NUM_47

// Boot Button
#define BUTTON_BOOT_PIN         GPIO_NUM_0

// I2S0 Pins (ES9039Q2M DAC - Output Only)
#define I2S0_MCLK_PIN           GPIO_NUM_13
#define I2S0_BCLK_PIN           GPIO_NUM_12
#define I2S0_WS_PIN             GPIO_NUM_10
#define I2S0_DOUT_PIN           GPIO_NUM_9
// Note: No I2S0_DIN - ES9039Q2M is DAC only, no ADC input

// I2C Bus (Sensor Hub)
#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_SDA_PIN             GPIO_NUM_7
#define I2C_SCL_PIN             GPIO_NUM_8
#define I2C_MASTER_FREQ_HZ      400000

// UART1 (SEN0395 mmWave Radar)
#define UART1_TX_PIN            GPIO_NUM_17
#define UART1_RX_PIN            GPIO_NUM_18
#define UART1_BAUD_RATE         115200

// UART2 (SEN0540 Formaldehyde Sensor)
#define UART2_TX_PIN            GPIO_NUM_19
#define UART2_RX_PIN            GPIO_NUM_20
#define UART2_BAUD_RATE         9600

// Display (MIPI-DSI)
#define DISPLAY_WIDTH           1024
#define DISPLAY_HEIGHT          600

// =============================================================================
// I2C Sensor Addresses
// =============================================================================
#define SCD41_ADDR              0x62    // CO2, Temperature, Humidity
#define SGP40_ADDR              0x59    // VOC Index
#define ENS160_ADDR             0x53    // Air Quality (eCO2, TVOC)
#define BMP388_ADDR             0x77    // Barometric Pressure, Temperature
#define SHT40_ADDR              0x44    // Temperature, Humidity (High precision)
#define SEN0428_ADDR            0x42    // Ambient Light Sensor

// =============================================================================
// Application State
// =============================================================================
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
#define DISPLAY_READY_BIT       BIT4
#define SENSORS_READY_BIT       BIT5

// =============================================================================
// Sensor Data Structure
// =============================================================================
typedef struct {
    // SCD41
    float co2_ppm;
    float scd41_temp;
    float scd41_humidity;

    // SGP40
    int32_t voc_index;

    // ENS160
    uint16_t eco2;
    uint16_t tvoc;
    uint8_t aqi;

    // BMP388
    float pressure_hpa;
    float bmp388_temp;

    // SHT40
    float sht40_temp;
    float sht40_humidity;

    // SEN0428
    uint16_t lux;

    // SEN0395 (mmWave)
    bool presence_detected;
    float distance_m;

    // SEN0540 (Formaldehyde)
    float hcho_mgm3;
} sensor_data_t;

static sensor_data_t sensor_data = {0};

// =============================================================================
// LED Control
// =============================================================================
static void set_led_state(app_state_t state)
{
    // TODO: Implement WS2812 RGB LED control with color patterns
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
            gpio_set_level(LED_STATUS_PIN, 1);
            break;
    }
}

// =============================================================================
// GPIO Initialization
// =============================================================================
static void IRAM_ATTR button_isr_handler(void *arg)
{
    if (current_state == STATE_IDLE) {
        xEventGroupSetBits(app_event_group, WAKE_WORD_DETECTED_BIT);
    }
}

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

    // Configure Boot Button with interrupt
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BUTTON_BOOT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&btn_conf));

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_BOOT_PIN, button_isr_handler, NULL);

    return ESP_OK;
}

// =============================================================================
// I2C Initialization (Sensor Hub)
// =============================================================================
static esp_err_t init_i2c(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));

    ESP_LOGI(TAG, "I2C Master initialized (SDA=%d, SCL=%d, %dkHz)",
             I2C_SDA_PIN, I2C_SCL_PIN, I2C_MASTER_FREQ_HZ / 1000);

    return ESP_OK;
}

// =============================================================================
// UART Initialization (UART Sensors)
// =============================================================================
static esp_err_t init_uart_sensors(void)
{
    // UART1 for SEN0395 (mmWave Radar)
    uart_config_t uart1_config = {
        .baud_rate = UART1_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 256, 256, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart1_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, UART1_TX_PIN, UART1_RX_PIN,
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG, "UART1 initialized for SEN0395 (mmWave)");

    // UART2 for SEN0540 (Formaldehyde)
    uart_config_t uart2_config = {
        .baud_rate = UART2_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, 256, 256, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart2_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, UART2_TX_PIN, UART2_RX_PIN,
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG, "UART2 initialized for SEN0540 (Formaldehyde)");

    return ESP_OK;
}

// =============================================================================
// Display Initialization (MIPI-DSI)
// =============================================================================
static esp_err_t init_display(void)
{
    ESP_LOGI(TAG, "Initializing MIPI-DSI Display: %dx%d", DISPLAY_WIDTH, DISPLAY_HEIGHT);

    // TODO: Initialize MIPI-DSI controller for 7" LCD
    // This requires ESP-IDF display driver configuration
    // esp_lcd_panel_handle_t panel_handle;
    // esp_lcd_dsi_bus_handle_t dsi_bus;

    // Placeholder for MIPI-DSI initialization
    // When implemented:
    // 1. Configure DSI bus
    // 2. Initialize panel driver
    // 3. Set up LVGL for UI rendering

    ESP_LOGI(TAG, "Display initialization placeholder (1024x600 MIPI-DSI)");
    xEventGroupSetBits(app_event_group, DISPLAY_READY_BIT);

    return ESP_OK;
}

// =============================================================================
// Audio Initialization
// =============================================================================
static esp_err_t init_audio(void)
{
    esp_err_t ret;

    // Initialize USB Audio for ReSpeaker USB Mic Array v2.0
    usb_audio_config_t usb_config = {
        .sample_rate = 16000,
        .channels = 1,
        .bits_per_sample = 16,
    };

    ret = usb_audio_init(&usb_config);
    if (ret == ESP_OK) {
        xEventGroupSetBits(app_event_group, USB_AUDIO_READY_BIT);
        ESP_LOGI(TAG, "USB Audio (ReSpeaker v2.0) initialized: 16kHz, 1ch, 16bit");
    } else {
        ESP_LOGE(TAG, "Failed to initialize USB Audio: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize I2S0 Output for ES9039Q2M DAC
    i2s_output_config_t i2s_config = {
        .mclk_pin = I2S0_MCLK_PIN,
        .bclk_pin = I2S0_BCLK_PIN,
        .ws_pin = I2S0_WS_PIN,
        .dout_pin = I2S0_DOUT_PIN,
        .sample_rate = 48000,       // ES9039Q2M supports high sample rates
        .bits_per_sample = 32,      // ES9039Q2M is 32-bit DAC
    };

    ret = i2s_output_init(&i2s_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "I2S0 Output (ES9039Q2M) initialized: 48kHz, 32bit");
    } else {
        ESP_LOGE(TAG, "Failed to initialize I2S Output: %s", esp_err_to_name(ret));
    }

    return ESP_OK;
}

// =============================================================================
// Audio Task
// =============================================================================
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

            // Read from ReSpeaker USB Mic Array v2.0
            if (usb_audio_read(audio_buffer, sizeof(audio_buffer), &bytes_read) == ESP_OK) {
                if (bytes_read > 0) {
                    // TODO: Process audio for wake word detection
                    // TODO: Send to Home Assistant for STT processing
                    ESP_LOGD(TAG, "Read %d bytes from USB microphone", bytes_read);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// =============================================================================
// Sensor Hub Task
// =============================================================================
static void sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Sensor Hub task started");
    ESP_LOGI(TAG, "Sensors: SCD41, SGP40, ENS160, BMP388, SHT40, SEN0428 (I2C)");
    ESP_LOGI(TAG, "Sensors: SEN0395 (UART1), SEN0540 (UART2)");

    xEventGroupSetBits(app_event_group, SENSORS_READY_BIT);

    while (1) {
        // TODO: Read I2C sensors
        // - SCD41: CO2, Temperature, Humidity
        // - SGP40: VOC Index
        // - ENS160: eCO2, TVOC, AQI
        // - BMP388: Pressure, Temperature
        // - SHT40: Temperature, Humidity (calibration reference)
        // - SEN0428: Ambient Light

        // TODO: Read UART sensors
        // - SEN0395: mmWave Radar (presence, distance)
        // - SEN0540: Formaldehyde (HCHO)

        // TODO: Send aggregated sensor data to Home Assistant
        // ha_client_send_sensor("sensor.omni_p4_co2", sensor_data.co2_ppm);
        // ha_client_send_sensor("sensor.omni_p4_voc", sensor_data.voc_index);
        // etc.

        ESP_LOGD(TAG, "Sensor read cycle completed");
        vTaskDelay(pdMS_TO_TICKS(30000));  // Read every 30 seconds
    }
}

// =============================================================================
// Home Assistant Task
// =============================================================================
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
        ha_client_process();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// =============================================================================
// Display Task
// =============================================================================
static void display_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Display task started (1024x600 MIPI-DSI)");

    // Wait for display to be ready
    xEventGroupWaitBits(app_event_group, DISPLAY_READY_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    while (1) {
        // TODO: Update display with:
        // - Current sensor readings
        // - Voice assistant status
        // - Air quality dashboard
        // - Clock/weather info

        vTaskDelay(pdMS_TO_TICKS(1000));  // Update display every second
    }
}

// =============================================================================
// Main Application Entry
// =============================================================================
void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Omni-P4 v4.1 (B&W Trace) Starting");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Hardware:");
    ESP_LOGI(TAG, "  - Mic: ReSpeaker USB Mic Array v2.0");
    ESP_LOGI(TAG, "  - DAC: ES9039Q2M (I2S0)");
    ESP_LOGI(TAG, "  - Display: 7\" MIPI-DSI (1024x600)");
    ESP_LOGI(TAG, "  - Sensors: 8 modules (I2C + UART)");
    ESP_LOGI(TAG, "========================================");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Create event group
    app_event_group = xEventGroupCreate();

    // Initialize peripherals
    ESP_ERROR_CHECK(init_gpio());
    ESP_LOGI(TAG, "GPIO initialized");

    ESP_ERROR_CHECK(init_i2c());
    ESP_LOGI(TAG, "I2C initialized (Sensor Hub)");

    ESP_ERROR_CHECK(init_uart_sensors());
    ESP_LOGI(TAG, "UART sensors initialized");

    ESP_ERROR_CHECK(init_display());
    ESP_LOGI(TAG, "Display initialized (1024x600)");

    // Initialize WiFi
    ESP_ERROR_CHECK(wifi_manager_init());
    xEventGroupSetBits(app_event_group, WIFI_CONNECTED_BIT);
    ESP_LOGI(TAG, "WiFi initialized");

    // Initialize Audio
    ESP_ERROR_CHECK(init_audio());
    ESP_LOGI(TAG, "Audio initialized (USB + I2S)");

    // Start tasks
    xTaskCreate(audio_task, "audio_task", 8192, NULL, 5, NULL);
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 3, NULL);
    xTaskCreate(ha_task, "ha_task", 4096, NULL, 4, NULL);
    xTaskCreate(display_task, "display_task", 4096, NULL, 2, NULL);

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  All tasks started. System ready.");
    ESP_LOGI(TAG, "========================================");
    set_led_state(STATE_IDLE);
}
