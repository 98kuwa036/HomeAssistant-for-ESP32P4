/**
 * USB Audio (UAC) Handler for ReSpeaker USB Mic Array
 *
 * Uses ESP-IDF USB Host UAC driver for ESP32-P4
 */

#include "usb_audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "usb/usb_host.h"

// Note: USB Host UAC driver is still experimental for ESP32-P4
// This code assumes the driver becomes available in future ESP-IDF versions

static const char *TAG = "USB_AUDIO";

// Ring buffer for audio data
static RingbufHandle_t audio_ringbuf = NULL;
static const size_t RINGBUF_SIZE = 32768;  // 32KB buffer

// Current configuration and state
static usb_audio_config_t current_config;
static usb_audio_state_t current_state = USB_AUDIO_STATE_DISCONNECTED;

// USB Host task handle
static TaskHandle_t usb_host_task_handle = NULL;

/**
 * USB Host library task
 * Handles USB device connection/disconnection events
 */
static void usb_host_lib_task(void *arg)
{
    ESP_LOGI(TAG, "USB Host library task started");

    // Install USB Host library
    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };

    esp_err_t ret = usb_host_install(&host_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB Host: %s", esp_err_to_name(ret));
        current_state = USB_AUDIO_STATE_ERROR;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "USB Host installed, waiting for devices...");

    while (1) {
        uint32_t event_flags;
        ret = usb_host_lib_handle_events(portMAX_DELAY, &event_flags);

        if (ret == ESP_OK) {
            if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
                ESP_LOGI(TAG, "No more clients");
            }
            if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
                ESP_LOGI(TAG, "All devices freed");
            }
        }
    }
}

/**
 * USB Audio Class client task
 * Handles UAC device communication
 */
static void uac_client_task(void *arg)
{
    ESP_LOGI(TAG, "UAC client task started");

    // TODO: Implement UAC client when ESP-IDF UAC Host driver is available
    // Current ESP-IDF v5.3 has experimental USB Host support for P4
    // Full UAC support may require esp-usb-host-uac component

    // Placeholder: Simulate device connection
    vTaskDelay(pdMS_TO_TICKS(1000));
    current_state = USB_AUDIO_STATE_CONNECTED;
    ESP_LOGI(TAG, "ReSpeaker USB Mic Array connected (simulated)");

    while (1) {
        // TODO: Read from UAC device and push to ring buffer
        // uac_host_read(buffer, size, &bytes_read, timeout);
        // xRingbufferSend(audio_ringbuf, buffer, bytes_read, 0);

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

esp_err_t usb_audio_init(const usb_audio_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Store configuration
    memcpy(&current_config, config, sizeof(usb_audio_config_t));

    ESP_LOGI(TAG, "Initializing USB Audio:");
    ESP_LOGI(TAG, "  Sample rate: %lu Hz", current_config.sample_rate);
    ESP_LOGI(TAG, "  Channels: %d", current_config.channels);
    ESP_LOGI(TAG, "  Bits per sample: %d", current_config.bits_per_sample);

    // Create ring buffer for audio data
    audio_ringbuf = xRingbufferCreate(RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);
    if (audio_ringbuf == NULL) {
        ESP_LOGE(TAG, "Failed to create audio ring buffer");
        return ESP_ERR_NO_MEM;
    }

    // Start USB Host library task
    BaseType_t ret = xTaskCreate(
        usb_host_lib_task,
        "usb_host_lib",
        4096,
        NULL,
        5,
        &usb_host_task_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create USB Host task");
        vRingbufferDelete(audio_ringbuf);
        return ESP_FAIL;
    }

    // Start UAC client task
    ret = xTaskCreate(
        uac_client_task,
        "uac_client",
        4096,
        NULL,
        4,
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UAC client task");
        vTaskDelete(usb_host_task_handle);
        vRingbufferDelete(audio_ringbuf);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "USB Audio initialized successfully");
    return ESP_OK;
}

void usb_audio_deinit(void)
{
    if (usb_host_task_handle != NULL) {
        vTaskDelete(usb_host_task_handle);
        usb_host_task_handle = NULL;
    }

    if (audio_ringbuf != NULL) {
        vRingbufferDelete(audio_ringbuf);
        audio_ringbuf = NULL;
    }

    usb_host_uninstall();
    current_state = USB_AUDIO_STATE_DISCONNECTED;

    ESP_LOGI(TAG, "USB Audio deinitialized");
}

esp_err_t usb_audio_read(uint8_t *buffer, size_t buffer_size, size_t *bytes_read)
{
    if (buffer == NULL || bytes_read == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (current_state != USB_AUDIO_STATE_STREAMING &&
        current_state != USB_AUDIO_STATE_CONNECTED) {
        *bytes_read = 0;
        return ESP_ERR_INVALID_STATE;
    }

    // Read from ring buffer
    size_t item_size;
    void *item = xRingbufferReceiveUpTo(
        audio_ringbuf,
        &item_size,
        pdMS_TO_TICKS(100),
        buffer_size
    );

    if (item != NULL) {
        memcpy(buffer, item, item_size);
        vRingbufferReturnItem(audio_ringbuf, item);
        *bytes_read = item_size;
        return ESP_OK;
    }

    *bytes_read = 0;
    return ESP_OK;  // No data available, but not an error
}

usb_audio_state_t usb_audio_get_state(void)
{
    return current_state;
}

esp_err_t usb_audio_start_stream(void)
{
    if (current_state != USB_AUDIO_STATE_CONNECTED) {
        return ESP_ERR_INVALID_STATE;
    }

    // TODO: Start UAC streaming
    current_state = USB_AUDIO_STATE_STREAMING;
    ESP_LOGI(TAG, "Audio streaming started");

    return ESP_OK;
}

esp_err_t usb_audio_stop_stream(void)
{
    if (current_state != USB_AUDIO_STATE_STREAMING) {
        return ESP_ERR_INVALID_STATE;
    }

    // TODO: Stop UAC streaming
    current_state = USB_AUDIO_STATE_CONNECTED;
    ESP_LOGI(TAG, "Audio streaming stopped");

    return ESP_OK;
}
