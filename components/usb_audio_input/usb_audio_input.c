/**
 * @file usb_audio_input.c
 * @brief USB Audio Input implementation using usb_host_uac
 *
 * Uses the espressif/usb_host_uac component (UAC 1.0 Host Driver)
 * to receive audio from USB microphones like ReSpeaker USB Mic Array.
 */

#include "usb_audio_input.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "usb/usb_host.h"
#include "usb_host_uac.h"

static const char *TAG = "usb_audio_in";

// ============================================================================
// ESP32-P4 Function EV Board USB VBUS Power Control
// ============================================================================

// GPIO 54 controls USB VBUS power on ESP32-P4 Function EV Board
#define USB_VBUS_EN_GPIO    54

/**
 * @brief Enable USB VBUS power
 *
 * On ESP32-P4 Function EV Board, GPIO 54 must be set HIGH
 * to supply power to the USB port.
 */
static esp_err_t usb_vbus_power_enable(void)
{
    ESP_LOGI(TAG, "Enabling USB VBUS power (GPIO %d)...", USB_VBUS_EN_GPIO);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << USB_VBUS_EN_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure VBUS GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = gpio_set_level(USB_VBUS_EN_GPIO, 1);  // HIGH = VBUS ON
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set VBUS GPIO high: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "USB VBUS power enabled");

    // Wait for power to stabilize
    vTaskDelay(pdMS_TO_TICKS(100));

    return ESP_OK;
}

/**
 * @brief Disable USB VBUS power
 */
static void usb_vbus_power_disable(void)
{
    gpio_set_level(USB_VBUS_EN_GPIO, 0);  // LOW = VBUS OFF
    ESP_LOGI(TAG, "USB VBUS power disabled");
}

// ============================================================================
// Internal State
// ============================================================================

// Internal context structure (different from public enum usb_audio_input_state_t)
typedef struct {
    // Callbacks
    usb_audio_input_data_cb_t data_cb;
    usb_audio_input_connect_cb_t connect_cb;
    void *user_ctx;

    // Configuration
    uint32_t preferred_sample_rate;
    uint8_t preferred_channels;

    // State (uses public enum type)
    usb_audio_input_state_t state;
    bool initialized;
    bool streaming;

    // Device info
    usb_audio_input_info_t device_info;
    uac_host_device_handle_t uac_device_handle;
    uint8_t device_addr;
    uint8_t iface_num;

    // Synchronization
    SemaphoreHandle_t mutex;
    TaskHandle_t usb_task_handle;

    // Event group for USB Host events
    EventGroupHandle_t event_group;
} usb_audio_ctx_t;

static usb_audio_ctx_t s_usb_audio = {0};

// Event bits
#define USB_HOST_LIB_EVENT      BIT0
#define USB_DEVICE_CONNECTED    BIT1
#define USB_DEVICE_DISCONNECTED BIT2

// ============================================================================
// USB Host Library Task
// ============================================================================

static void usb_host_lib_task(void *arg)
{
    ESP_LOGI(TAG, "USB Host library task started");

    while (s_usb_audio.initialized) {
        uint32_t event_flags;
        esp_err_t ret = usb_host_lib_handle_events(portMAX_DELAY, &event_flags);

        if (ret == ESP_OK) {
            if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
                ESP_LOGD(TAG, "No more USB clients");
            }
            if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
                ESP_LOGD(TAG, "All USB devices freed");
            }
        } else if (ret != ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "USB host lib error: %s", esp_err_to_name(ret));
        }
    }

    ESP_LOGI(TAG, "USB Host library task ended");
    vTaskDelete(NULL);
}

// ============================================================================
// Audio Downsampling Buffer (48kHz stereo → 16kHz mono)
// ============================================================================

// Static buffer for downsampled audio
// Max USB packet ~1024 bytes, after 1/6 reduction = ~170 samples
#define DOWNSAMPLE_BUFFER_SIZE      1024
static int16_t s_downsample_buffer[DOWNSAMPLE_BUFFER_SIZE];

// ============================================================================
// UAC Host Callbacks
// ============================================================================

/**
 * @brief UAC device event callback
 */
static void uac_device_callback(uac_host_device_handle_t uac_device_handle,
                                 const uac_host_device_event_t event,
                                 void *arg)
{
    switch (event) {
        case UAC_HOST_DEVICE_EVENT_RX_DONE: {
            // Audio data received from USB microphone
            uac_host_transfer_t transfer = {0};
            esp_err_t ret = uac_host_get_rx_transfer(uac_device_handle, &transfer);
            if (ret == ESP_OK && transfer.data && transfer.actual_num_bytes > 0) {
                // --- Downsampling: 48kHz stereo → 16kHz mono ---
                uint32_t in_rate = s_usb_audio.device_info.sample_rate;
                uint32_t channels = s_usb_audio.device_info.channels;

                if (in_rate == 48000 && s_usb_audio.data_cb) {
                    const int16_t *in_samples = (const int16_t *)transfer.data;
                    size_t num_frames = transfer.actual_num_bytes / (sizeof(int16_t) * channels);
                    size_t out_idx = 0;

                    // Take every 3rd frame (48kHz / 3 = 16kHz)
                    for (size_t i = 0; i < num_frames; i += 3) {
                        if (channels >= 2) {
                            // Stereo: use left channel only
                            s_downsample_buffer[out_idx++] = in_samples[i * 2];
                        } else {
                            // Mono: simple decimation
                            s_downsample_buffer[out_idx++] = in_samples[i];
                        }
                    }

                    size_t out_bytes = out_idx * sizeof(int16_t);
                    s_usb_audio.data_cb((const uint8_t *)s_downsample_buffer,
                                        out_bytes, s_usb_audio.user_ctx);
                } else if (s_usb_audio.data_cb) {
                    // Non-48kHz: pass through unchanged
                    s_usb_audio.data_cb(transfer.data, transfer.actual_num_bytes,
                                        s_usb_audio.user_ctx);
                }
            }
            break;
        }

        case UAC_HOST_DEVICE_EVENT_TX_DONE:
            // TX done (we don't use TX for input)
            break;

        case UAC_HOST_DEVICE_EVENT_TRANSFER_ERROR:
            ESP_LOGW(TAG, "USB transfer error");
            break;

        case UAC_HOST_DRIVER_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "USB Audio device disconnected");
            s_usb_audio.state = USB_AUDIO_INPUT_STATE_WAITING;
            s_usb_audio.streaming = false;
            s_usb_audio.uac_device_handle = NULL;

            if (s_usb_audio.connect_cb) {
                s_usb_audio.connect_cb(false, NULL, s_usb_audio.user_ctx);
            }

            xEventGroupSetBits(s_usb_audio.event_group, USB_DEVICE_DISCONNECTED);
            break;

        default:
            ESP_LOGD(TAG, "UAC device event: %d", event);
            break;
    }
}

/**
 * @brief UAC driver event callback (called when new device connects)
 */
static void uac_driver_callback(uint8_t addr, uint8_t iface_num,
                                 const uac_host_driver_event_t event,
                                 void *arg)
{
    switch (event) {
        case UAC_HOST_DRIVER_EVENT_RX_CONNECTED: {
            // Microphone connected!
            ESP_LOGI(TAG, "USB Microphone connected (addr=%d, iface=%d)", addr, iface_num);

            s_usb_audio.device_addr = addr;
            s_usb_audio.iface_num = iface_num;

            // Open the UAC device
            uac_host_dev_info_t dev_info;
            const uac_host_device_config_t dev_config = {
                .addr = addr,
                .iface_num = iface_num,
                .buffer_size = 16 * 1024,  // 16KB buffer
                .buffer_threshold = 4 * 1024,
                .callback = uac_device_callback,
                .callback_arg = NULL,
            };

            esp_err_t ret = uac_host_device_open(&dev_config, &s_usb_audio.uac_device_handle);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to open UAC device: %s", esp_err_to_name(ret));
                return;
            }

            // Get device info
            ret = uac_host_get_device_info(s_usb_audio.uac_device_handle, &dev_info);
            if (ret == ESP_OK) {
                s_usb_audio.device_info.vid = dev_info.VID;
                s_usb_audio.device_info.pid = dev_info.PID;
                s_usb_audio.device_info.sample_rate = 48000;  // Default, will be updated
                s_usb_audio.device_info.channels = dev_info.channels;
                s_usb_audio.device_info.bit_depth = dev_info.bit_resolution;
                s_usb_audio.device_info.is_respeaker =
                    (dev_info.VID == RESPEAKER_VID && dev_info.PID == RESPEAKER_PID);

                ESP_LOGI(TAG, "Device: VID=0x%04X, PID=0x%04X, %d ch, %d-bit",
                         dev_info.VID, dev_info.PID,
                         dev_info.channels, dev_info.bit_resolution);

                if (s_usb_audio.device_info.is_respeaker) {
                    ESP_LOGI(TAG, "ReSpeaker USB Mic Array detected!");
                }
            }

            // Set sample rate
            uint32_t sample_rate = s_usb_audio.preferred_sample_rate;
            if (sample_rate == 0) {
                sample_rate = 48000;  // Default to 48kHz
            }

            ret = uac_host_device_set_interface(s_usb_audio.uac_device_handle, sample_rate);
            if (ret == ESP_OK) {
                s_usb_audio.device_info.sample_rate = sample_rate;
                ESP_LOGI(TAG, "Sample rate set to %lu Hz", sample_rate);
            } else {
                ESP_LOGW(TAG, "Failed to set sample rate: %s", esp_err_to_name(ret));
            }

            s_usb_audio.state = USB_AUDIO_INPUT_STATE_CONNECTED;

            // Notify user
            if (s_usb_audio.connect_cb) {
                s_usb_audio.connect_cb(true, &s_usb_audio.device_info, s_usb_audio.user_ctx);
            }

            xEventGroupSetBits(s_usb_audio.event_group, USB_DEVICE_CONNECTED);
            break;
        }

        case UAC_HOST_DRIVER_EVENT_TX_CONNECTED:
            // Speaker connected (we only care about microphone)
            ESP_LOGI(TAG, "USB Speaker connected (addr=%d, iface=%d) - ignored", addr, iface_num);
            break;

        case UAC_HOST_DRIVER_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "UAC driver: device disconnected (addr=%d)", addr);
            break;

        default:
            ESP_LOGD(TAG, "UAC driver event: %d", event);
            break;
    }
}

// ============================================================================
// Public API Implementation
// ============================================================================

esp_err_t usb_audio_input_init(const usb_audio_input_config_t *config)
{
    if (s_usb_audio.initialized) {
        ESP_LOGW(TAG, "USB Audio Input already initialized");
        return ESP_OK;
    }

    if (!config || !config->data_cb) {
        ESP_LOGE(TAG, "Invalid config: data callback is required");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing USB Audio Input...");

    // Enable USB VBUS power (required for ESP32-P4 Function EV Board)
    esp_err_t ret = usb_vbus_power_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable USB VBUS power");
        return ret;
    }

    // Store configuration
    s_usb_audio.data_cb = config->data_cb;
    s_usb_audio.connect_cb = config->connect_cb;
    s_usb_audio.user_ctx = config->user_ctx;
    s_usb_audio.preferred_sample_rate = config->preferred_sample_rate;
    s_usb_audio.preferred_channels = config->preferred_channels;

    // Create synchronization primitives
    s_usb_audio.mutex = xSemaphoreCreateMutex();
    s_usb_audio.event_group = xEventGroupCreate();
    if (!s_usb_audio.mutex || !s_usb_audio.event_group) {
        ESP_LOGE(TAG, "Failed to create sync primitives");
        return ESP_ERR_NO_MEM;
    }

    // Install USB Host Library
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };

    ret = usb_host_install(&host_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB Host install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Start USB Host library task
    s_usb_audio.initialized = true;  // Set before creating task
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        usb_host_lib_task,
        "usb_host_lib",
        4096,
        NULL,
        2,  // Priority
        &s_usb_audio.usb_task_handle,
        0   // Core 0
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create USB host task");
        usb_host_uninstall();
        s_usb_audio.initialized = false;
        return ESP_FAIL;
    }

    // Install UAC Host driver
    const uac_host_driver_config_t uac_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = uac_driver_callback,
        .callback_arg = NULL,
    };

    ret = uac_host_install(&uac_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UAC Host install failed: %s", esp_err_to_name(ret));
        s_usb_audio.initialized = false;
        vTaskDelete(s_usb_audio.usb_task_handle);
        usb_host_uninstall();
        return ret;
    }

    s_usb_audio.state = USB_AUDIO_INPUT_STATE_WAITING;

    ESP_LOGI(TAG, "USB Audio Input initialized");
    ESP_LOGI(TAG, "  Waiting for USB microphone connection...");
    ESP_LOGI(TAG, "  Supported: ReSpeaker USB Mic Array, UAC 1.0 devices");

    return ESP_OK;
}

void usb_audio_input_deinit(void)
{
    if (!s_usb_audio.initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing USB Audio Input...");

    // Stop streaming
    usb_audio_input_stop();

    // Close device if open
    if (s_usb_audio.uac_device_handle) {
        uac_host_device_close(s_usb_audio.uac_device_handle);
        s_usb_audio.uac_device_handle = NULL;
    }

    // Uninstall UAC driver
    uac_host_uninstall();

    // Stop USB host task
    s_usb_audio.initialized = false;
    if (s_usb_audio.usb_task_handle) {
        // The task will exit on its own
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Uninstall USB Host
    usb_host_uninstall();

    // Disable USB VBUS power
    usb_vbus_power_disable();

    // Clean up
    if (s_usb_audio.mutex) {
        vSemaphoreDelete(s_usb_audio.mutex);
    }
    if (s_usb_audio.event_group) {
        vEventGroupDelete(s_usb_audio.event_group);
    }

    memset(&s_usb_audio, 0, sizeof(s_usb_audio));
    ESP_LOGI(TAG, "USB Audio Input deinitialized");
}

esp_err_t usb_audio_input_start(void)
{
    if (!s_usb_audio.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_usb_audio.state != USB_AUDIO_INPUT_STATE_CONNECTED) {
        ESP_LOGW(TAG, "No USB audio device connected");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_usb_audio.streaming) {
        ESP_LOGW(TAG, "Already streaming");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting USB audio streaming...");

    esp_err_t ret = uac_host_device_start(s_usb_audio.uac_device_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start streaming: %s", esp_err_to_name(ret));
        return ret;
    }

    s_usb_audio.streaming = true;
    s_usb_audio.state = USB_AUDIO_INPUT_STATE_STREAMING;

    ESP_LOGI(TAG, "USB audio streaming started");
    ESP_LOGI(TAG, "  Format: %lu Hz, %d ch, %d-bit",
             s_usb_audio.device_info.sample_rate,
             s_usb_audio.device_info.channels,
             s_usb_audio.device_info.bit_depth);

    return ESP_OK;
}

esp_err_t usb_audio_input_stop(void)
{
    if (!s_usb_audio.initialized || !s_usb_audio.streaming) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping USB audio streaming...");

    if (s_usb_audio.uac_device_handle) {
        uac_host_device_stop(s_usb_audio.uac_device_handle);
    }

    s_usb_audio.streaming = false;
    if (s_usb_audio.state == USB_AUDIO_INPUT_STATE_STREAMING) {
        s_usb_audio.state = USB_AUDIO_INPUT_STATE_CONNECTED;
    }

    ESP_LOGI(TAG, "USB audio streaming stopped");
    return ESP_OK;
}

usb_audio_input_state_t usb_audio_input_get_state(void)
{
    return s_usb_audio.state;
}

esp_err_t usb_audio_input_get_info(usb_audio_input_info_t *info)
{
    if (!info) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_usb_audio.state < USB_AUDIO_INPUT_STATE_CONNECTED) {
        return ESP_ERR_INVALID_STATE;
    }

    memcpy(info, &s_usb_audio.device_info, sizeof(usb_audio_input_info_t));
    return ESP_OK;
}

bool usb_audio_input_is_ready(void)
{
    return s_usb_audio.state == USB_AUDIO_INPUT_STATE_STREAMING;
}

bool usb_audio_input_is_respeaker(void)
{
    return s_usb_audio.state >= USB_AUDIO_INPUT_STATE_CONNECTED &&
           s_usb_audio.device_info.is_respeaker;
}
