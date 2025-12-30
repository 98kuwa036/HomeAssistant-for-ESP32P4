/**
 * @file usb_audio_host.c
 * @brief USB Audio Class (UAC) Host Driver Implementation
 *
 * Supports USB Audio Class 1.0 devices like ReSpeaker USB Mic Array v2.0
 */

#include "usb_audio_host.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "usb/usb_host.h"
#include "usb/usb_types_ch9.h"

static const char *TAG = "usb_audio";

// ============================================================================
// USB Audio Class Definitions
// ============================================================================

// USB Audio Class codes
#define USB_CLASS_AUDIO             0x01
#define USB_SUBCLASS_AUDIOCONTROL   0x01
#define USB_SUBCLASS_AUDIOSTREAMING 0x02

// Audio Class-Specific Descriptor Types
#define USB_CS_INTERFACE            0x24
#define USB_CS_ENDPOINT             0x25

// AudioStreaming Interface Descriptor Subtypes
#define USB_AS_GENERAL              0x01
#define USB_AS_FORMAT_TYPE          0x02

// Audio Data Format Type I
#define USB_AUDIO_FORMAT_TYPE_I     0x01

// ============================================================================
// Internal State
// ============================================================================

typedef struct {
    // Configuration
    usb_audio_host_config_t config;

    // State
    usb_audio_state_t state;
    bool initialized;

    // USB Host
    usb_host_client_handle_t client_handle;
    usb_device_handle_t device_handle;
    uint8_t interface_num;
    uint8_t alt_setting;
    uint8_t ep_addr;

    // Device info
    usb_audio_device_info_t device_info;

    // Ring buffer for audio data
    uint8_t *ring_buffer;
    size_t ring_size;
    size_t write_pos;
    size_t read_pos;
    SemaphoreHandle_t buffer_mutex;

    // Tasks
    TaskHandle_t host_task;
    TaskHandle_t stream_task;
    bool task_running;

    // Transfer
    usb_transfer_t *transfer;

    // Events
    EventGroupHandle_t events;

} usb_audio_host_state_t;

static usb_audio_host_state_t s_usb_audio = {0};

// Event bits
#define EVENT_DEVICE_CONNECTED      BIT0
#define EVENT_DEVICE_DISCONNECTED   BIT1
#define EVENT_TRANSFER_COMPLETE     BIT2
#define EVENT_STOP_REQUEST          BIT3

// ============================================================================
// VBUS Power Control
// ============================================================================

static esp_err_t enable_vbus_power(void)
{
    if (s_usb_audio.config.vbus_gpio < 0) {
        ESP_LOGI(TAG, "VBUS control disabled, assuming external power");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Enabling VBUS power on GPIO%d", s_usb_audio.config.vbus_gpio);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << s_usb_audio.config.vbus_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "GPIO config failed");
    ESP_RETURN_ON_ERROR(
        gpio_set_level(s_usb_audio.config.vbus_gpio, s_usb_audio.config.vbus_active_level),
        TAG, "GPIO set level failed"
    );

    // Wait for device power-up
    vTaskDelay(pdMS_TO_TICKS(500));

    return ESP_OK;
}

static void disable_vbus_power(void)
{
    if (s_usb_audio.config.vbus_gpio >= 0) {
        gpio_set_level(s_usb_audio.config.vbus_gpio, !s_usb_audio.config.vbus_active_level);
    }
}

// ============================================================================
// Ring Buffer Operations
// ============================================================================

static size_t ring_buffer_write(const uint8_t *data, size_t len)
{
    if (!s_usb_audio.ring_buffer || !s_usb_audio.buffer_mutex) return 0;

    if (xSemaphoreTake(s_usb_audio.buffer_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return 0;
    }

    size_t space = (s_usb_audio.read_pos - s_usb_audio.write_pos - 1 +
                    s_usb_audio.ring_size) % s_usb_audio.ring_size;
    size_t to_write = (len < space) ? len : space;

    if (to_write > 0) {
        size_t first_chunk = s_usb_audio.ring_size - s_usb_audio.write_pos;
        if (to_write <= first_chunk) {
            memcpy(s_usb_audio.ring_buffer + s_usb_audio.write_pos, data, to_write);
        } else {
            memcpy(s_usb_audio.ring_buffer + s_usb_audio.write_pos, data, first_chunk);
            memcpy(s_usb_audio.ring_buffer, data + first_chunk, to_write - first_chunk);
        }
        s_usb_audio.write_pos = (s_usb_audio.write_pos + to_write) % s_usb_audio.ring_size;
    }

    xSemaphoreGive(s_usb_audio.buffer_mutex);
    return to_write;
}

static size_t ring_buffer_read(uint8_t *data, size_t len)
{
    if (!s_usb_audio.ring_buffer || !s_usb_audio.buffer_mutex) return 0;

    if (xSemaphoreTake(s_usb_audio.buffer_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return 0;
    }

    size_t available = (s_usb_audio.write_pos - s_usb_audio.read_pos +
                        s_usb_audio.ring_size) % s_usb_audio.ring_size;
    size_t to_read = (len < available) ? len : available;

    if (to_read > 0) {
        size_t first_chunk = s_usb_audio.ring_size - s_usb_audio.read_pos;
        if (to_read <= first_chunk) {
            memcpy(data, s_usb_audio.ring_buffer + s_usb_audio.read_pos, to_read);
        } else {
            memcpy(data, s_usb_audio.ring_buffer + s_usb_audio.read_pos, first_chunk);
            memcpy(data + first_chunk, s_usb_audio.ring_buffer, to_read - first_chunk);
        }
        s_usb_audio.read_pos = (s_usb_audio.read_pos + to_read) % s_usb_audio.ring_size;
    }

    xSemaphoreGive(s_usb_audio.buffer_mutex);
    return to_read;
}

static size_t ring_buffer_available(void)
{
    if (!s_usb_audio.ring_buffer || !s_usb_audio.buffer_mutex) return 0;

    if (xSemaphoreTake(s_usb_audio.buffer_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return 0;
    }

    size_t available = (s_usb_audio.write_pos - s_usb_audio.read_pos +
                        s_usb_audio.ring_size) % s_usb_audio.ring_size;

    xSemaphoreGive(s_usb_audio.buffer_mutex);
    return available;
}

// ============================================================================
// USB Transfer Callback
// ============================================================================

static void transfer_callback(usb_transfer_t *transfer)
{
    if (transfer->status == USB_TRANSFER_STATUS_COMPLETED) {
        // Write received audio data to ring buffer
        if (transfer->actual_num_bytes > 0) {
            size_t written = ring_buffer_write(transfer->data_buffer, transfer->actual_num_bytes);

            // Call user callback if registered
            if (s_usb_audio.config.data_cb && written > 0) {
                s_usb_audio.config.data_cb(
                    transfer->data_buffer,
                    transfer->actual_num_bytes,
                    s_usb_audio.config.user_ctx
                );
            }
        }
    } else {
        ESP_LOGW(TAG, "Transfer failed: status=%d", transfer->status);
    }

    xEventGroupSetBits(s_usb_audio.events, EVENT_TRANSFER_COMPLETE);
}

// ============================================================================
// USB Host Client Callback
// ============================================================================

static void client_event_callback(const usb_host_client_event_msg_t *event_msg, void *arg)
{
    switch (event_msg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            ESP_LOGI(TAG, "New USB device connected: address=%d", event_msg->new_dev.address);
            xEventGroupSetBits(s_usb_audio.events, EVENT_DEVICE_CONNECTED);
            break;

        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            ESP_LOGW(TAG, "USB device disconnected");
            s_usb_audio.state = USB_AUDIO_STATE_WAITING;
            xEventGroupSetBits(s_usb_audio.events, EVENT_DEVICE_DISCONNECTED);

            if (s_usb_audio.config.connect_cb) {
                s_usb_audio.config.connect_cb(false, NULL, s_usb_audio.config.user_ctx);
            }
            break;

        default:
            break;
    }
}

// ============================================================================
// Device Enumeration and Configuration
// ============================================================================

static esp_err_t get_device_info(void)
{
    const usb_device_desc_t *dev_desc;
    ESP_RETURN_ON_ERROR(
        usb_host_get_device_descriptor(s_usb_audio.device_handle, &dev_desc),
        TAG, "Failed to get device descriptor"
    );

    s_usb_audio.device_info.vid = dev_desc->idVendor;
    s_usb_audio.device_info.pid = dev_desc->idProduct;

    // Check if this is a ReSpeaker
    if (dev_desc->idVendor == RESPEAKER_VID && dev_desc->idProduct == RESPEAKER_PID) {
        ESP_LOGI(TAG, "ReSpeaker USB Mic Array v2.0 detected!");
        strncpy(s_usb_audio.device_info.product, "ReSpeaker USB Mic Array v2.0", 63);
    } else {
        ESP_LOGI(TAG, "USB Audio device: VID=0x%04X PID=0x%04X",
                 dev_desc->idVendor, dev_desc->idProduct);
    }

    // Get string descriptors
    usb_device_info_t dev_info;
    if (usb_host_device_info(s_usb_audio.device_handle, &dev_info) == ESP_OK) {
        // String descriptors would be fetched here if needed
    }

    // Set default audio parameters (ReSpeaker defaults)
    s_usb_audio.device_info.sample_rate = USB_AUDIO_SAMPLE_RATE;
    s_usb_audio.device_info.channels = USB_AUDIO_CHANNELS;
    s_usb_audio.device_info.bit_depth = USB_AUDIO_BIT_DEPTH;

    return ESP_OK;
}

static esp_err_t find_audio_interface(void)
{
    const usb_config_desc_t *config_desc;
    ESP_RETURN_ON_ERROR(
        usb_host_get_active_config_descriptor(s_usb_audio.device_handle, &config_desc),
        TAG, "Failed to get config descriptor"
    );

    const uint8_t *p = (const uint8_t *)config_desc;
    const uint8_t *end = p + config_desc->wTotalLength;

    bool found_audio_streaming = false;
    uint8_t current_interface = 0;
    uint8_t current_alt = 0;

    while (p < end) {
        uint8_t len = p[0];
        uint8_t type = p[1];

        if (len == 0) break;

        if (type == USB_B_DESCRIPTOR_TYPE_INTERFACE) {
            const usb_intf_desc_t *intf = (const usb_intf_desc_t *)p;
            current_interface = intf->bInterfaceNumber;
            current_alt = intf->bAlternateSetting;

            if (intf->bInterfaceClass == USB_CLASS_AUDIO &&
                intf->bInterfaceSubClass == USB_SUBCLASS_AUDIOSTREAMING &&
                intf->bNumEndpoints > 0) {
                ESP_LOGI(TAG, "Found AudioStreaming interface: %d alt: %d",
                         current_interface, current_alt);
                s_usb_audio.interface_num = current_interface;
                s_usb_audio.alt_setting = current_alt;
                found_audio_streaming = true;
            }
        } else if (type == USB_B_DESCRIPTOR_TYPE_ENDPOINT && found_audio_streaming) {
            const usb_ep_desc_t *ep = (const usb_ep_desc_t *)p;

            // Look for isochronous IN endpoint (microphone)
            if ((ep->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK) &&
                (ep->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_ISOC) {
                s_usb_audio.ep_addr = ep->bEndpointAddress;
                ESP_LOGI(TAG, "Found audio IN endpoint: 0x%02X, maxPacket: %d",
                         ep->bEndpointAddress, ep->wMaxPacketSize);
                return ESP_OK;
            }
        }

        p += len;
    }

    ESP_LOGE(TAG, "No suitable audio endpoint found");
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t configure_audio_interface(void)
{
    // Claim interface
    ESP_RETURN_ON_ERROR(
        usb_host_interface_claim(
            s_usb_audio.client_handle,
            s_usb_audio.device_handle,
            s_usb_audio.interface_num,
            s_usb_audio.alt_setting
        ),
        TAG, "Failed to claim interface"
    );

    ESP_LOGI(TAG, "Audio interface claimed: %d alt: %d",
             s_usb_audio.interface_num, s_usb_audio.alt_setting);

    return ESP_OK;
}

// ============================================================================
// USB Host Task
// ============================================================================

static void usb_host_task(void *arg)
{
    ESP_LOGI(TAG, "USB Host task started");

    // Install USB Host Library
    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };

    esp_err_t ret = usb_host_install(&host_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB Host: %s", esp_err_to_name(ret));
        s_usb_audio.state = USB_AUDIO_STATE_ERROR;
        goto exit;
    }

    // Register client
    usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {
            .client_event_callback = client_event_callback,
            .callback_arg = NULL,
        },
    };

    ret = usb_host_client_register(&client_config, &s_usb_audio.client_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register USB Host client: %s", esp_err_to_name(ret));
        s_usb_audio.state = USB_AUDIO_STATE_ERROR;
        goto cleanup_host;
    }

    s_usb_audio.state = USB_AUDIO_STATE_WAITING;
    ESP_LOGI(TAG, "Waiting for USB audio device...");

    while (s_usb_audio.task_running) {
        // Handle USB Host events
        usb_host_client_handle_events(s_usb_audio.client_handle, pdMS_TO_TICKS(100));

        EventBits_t bits = xEventGroupWaitBits(
            s_usb_audio.events,
            EVENT_DEVICE_CONNECTED | EVENT_DEVICE_DISCONNECTED | EVENT_STOP_REQUEST,
            pdTRUE,
            pdFALSE,
            0
        );

        if (bits & EVENT_STOP_REQUEST) {
            ESP_LOGI(TAG, "Stop request received");
            break;
        }

        if (bits & EVENT_DEVICE_CONNECTED) {
            // Open device
            ret = usb_host_device_open(s_usb_audio.client_handle, 1, &s_usb_audio.device_handle);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to open device: %s", esp_err_to_name(ret));
                continue;
            }

            // Get device info
            ret = get_device_info();
            if (ret != ESP_OK) {
                usb_host_device_close(s_usb_audio.client_handle, s_usb_audio.device_handle);
                continue;
            }

            // Find audio interface
            ret = find_audio_interface();
            if (ret != ESP_OK) {
                usb_host_device_close(s_usb_audio.client_handle, s_usb_audio.device_handle);
                continue;
            }

            // Configure interface
            ret = configure_audio_interface();
            if (ret != ESP_OK) {
                usb_host_device_close(s_usb_audio.client_handle, s_usb_audio.device_handle);
                continue;
            }

            s_usb_audio.state = USB_AUDIO_STATE_CONNECTED;
            ESP_LOGI(TAG, "USB audio device ready");

            if (s_usb_audio.config.connect_cb) {
                s_usb_audio.config.connect_cb(true, &s_usb_audio.device_info, s_usb_audio.config.user_ctx);
            }
        }

        if (bits & EVENT_DEVICE_DISCONNECTED) {
            if (s_usb_audio.device_handle) {
                usb_host_device_close(s_usb_audio.client_handle, s_usb_audio.device_handle);
                s_usb_audio.device_handle = NULL;
            }
            s_usb_audio.state = USB_AUDIO_STATE_WAITING;
        }

        // USB Host library event handling
        usb_host_lib_handle_events(pdMS_TO_TICKS(10), NULL);
    }

    // Cleanup
    if (s_usb_audio.device_handle) {
        usb_host_interface_release(s_usb_audio.client_handle, s_usb_audio.device_handle, s_usb_audio.interface_num);
        usb_host_device_close(s_usb_audio.client_handle, s_usb_audio.device_handle);
    }

    usb_host_client_deregister(s_usb_audio.client_handle);

cleanup_host:
    usb_host_uninstall();

exit:
    ESP_LOGI(TAG, "USB Host task exiting");
    vTaskDelete(NULL);
}

// ============================================================================
// Audio Streaming Task
// ============================================================================

static void audio_stream_task(void *arg)
{
    ESP_LOGI(TAG, "Audio stream task started");

    // Allocate transfer
    usb_host_transfer_alloc(USB_AUDIO_PACKET_SIZE * 8, 0, &s_usb_audio.transfer);
    if (!s_usb_audio.transfer) {
        ESP_LOGE(TAG, "Failed to allocate transfer");
        vTaskDelete(NULL);
        return;
    }

    s_usb_audio.transfer->device_handle = s_usb_audio.device_handle;
    s_usb_audio.transfer->bEndpointAddress = s_usb_audio.ep_addr;
    s_usb_audio.transfer->callback = transfer_callback;
    s_usb_audio.transfer->context = NULL;
    s_usb_audio.transfer->num_bytes = USB_AUDIO_PACKET_SIZE * 8;

    s_usb_audio.state = USB_AUDIO_STATE_STREAMING;
    ESP_LOGI(TAG, "Starting audio capture at %d Hz, %d-bit, %d channels",
             USB_AUDIO_SAMPLE_RATE, USB_AUDIO_BIT_DEPTH, USB_AUDIO_CHANNELS);

    while (s_usb_audio.task_running && s_usb_audio.state == USB_AUDIO_STATE_STREAMING) {
        // Submit isochronous transfer
        esp_err_t ret = usb_host_transfer_submit(s_usb_audio.transfer);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Transfer submit failed: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Wait for transfer completion
        EventBits_t bits = xEventGroupWaitBits(
            s_usb_audio.events,
            EVENT_TRANSFER_COMPLETE | EVENT_STOP_REQUEST | EVENT_DEVICE_DISCONNECTED,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(100)
        );

        if (bits & (EVENT_STOP_REQUEST | EVENT_DEVICE_DISCONNECTED)) {
            break;
        }
    }

    // Cleanup
    usb_host_transfer_free(s_usb_audio.transfer);
    s_usb_audio.transfer = NULL;

    ESP_LOGI(TAG, "Audio stream task exiting");
    vTaskDelete(NULL);
}

// ============================================================================
// Public API Implementation
// ============================================================================

esp_err_t usb_audio_host_init(const usb_audio_host_config_t *config)
{
    if (s_usb_audio.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing USB Audio Host...");

    // Store config
    memcpy(&s_usb_audio.config, config, sizeof(usb_audio_host_config_t));

    // Create synchronization primitives
    s_usb_audio.events = xEventGroupCreate();
    s_usb_audio.buffer_mutex = xSemaphoreCreateMutex();

    if (!s_usb_audio.events || !s_usb_audio.buffer_mutex) {
        ESP_LOGE(TAG, "Failed to create sync primitives");
        return ESP_ERR_NO_MEM;
    }

    // Allocate ring buffer
    s_usb_audio.ring_size = USB_AUDIO_RING_BUFFER_SIZE;
    s_usb_audio.ring_buffer = heap_caps_malloc(s_usb_audio.ring_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_usb_audio.ring_buffer) {
        // Fallback to internal memory
        s_usb_audio.ring_buffer = malloc(s_usb_audio.ring_size);
    }

    if (!s_usb_audio.ring_buffer) {
        ESP_LOGE(TAG, "Failed to allocate ring buffer");
        return ESP_ERR_NO_MEM;
    }

    s_usb_audio.write_pos = 0;
    s_usb_audio.read_pos = 0;

    // Enable VBUS power
    esp_err_t ret = enable_vbus_power();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "VBUS enable failed, continuing...");
    }

    // Start USB Host task
    s_usb_audio.task_running = true;
    xTaskCreatePinnedToCore(
        usb_host_task,
        "usb_host",
        4096,
        NULL,
        5,
        &s_usb_audio.host_task,
        0  // Core 0
    );

    s_usb_audio.initialized = true;
    s_usb_audio.state = USB_AUDIO_STATE_WAITING;

    ESP_LOGI(TAG, "USB Audio Host initialized, waiting for device...");
    return ESP_OK;
}

void usb_audio_host_deinit(void)
{
    if (!s_usb_audio.initialized) return;

    ESP_LOGI(TAG, "Deinitializing USB Audio Host...");

    // Signal tasks to stop
    s_usb_audio.task_running = false;
    xEventGroupSetBits(s_usb_audio.events, EVENT_STOP_REQUEST);

    // Wait for tasks to finish
    vTaskDelay(pdMS_TO_TICKS(500));

    // Disable VBUS
    disable_vbus_power();

    // Free resources
    if (s_usb_audio.ring_buffer) {
        free(s_usb_audio.ring_buffer);
    }

    if (s_usb_audio.events) {
        vEventGroupDelete(s_usb_audio.events);
    }

    if (s_usb_audio.buffer_mutex) {
        vSemaphoreDelete(s_usb_audio.buffer_mutex);
    }

    memset(&s_usb_audio, 0, sizeof(s_usb_audio));
}

esp_err_t usb_audio_host_start(void)
{
    if (!s_usb_audio.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_usb_audio.state != USB_AUDIO_STATE_CONNECTED) {
        ESP_LOGE(TAG, "Device not connected");
        return ESP_ERR_INVALID_STATE;
    }

    // Start streaming task
    xTaskCreatePinnedToCore(
        audio_stream_task,
        "usb_audio_stream",
        4096,
        NULL,
        6,  // Higher priority for audio
        &s_usb_audio.stream_task,
        1  // Core 1
    );

    return ESP_OK;
}

esp_err_t usb_audio_host_stop(void)
{
    if (s_usb_audio.state == USB_AUDIO_STATE_STREAMING) {
        xEventGroupSetBits(s_usb_audio.events, EVENT_STOP_REQUEST);
        s_usb_audio.state = USB_AUDIO_STATE_CONNECTED;
    }
    return ESP_OK;
}

usb_audio_state_t usb_audio_host_get_state(void)
{
    return s_usb_audio.state;
}

esp_err_t usb_audio_host_get_device_info(usb_audio_device_info_t *info)
{
    if (!info) return ESP_ERR_INVALID_ARG;

    if (s_usb_audio.state < USB_AUDIO_STATE_CONNECTED) {
        return ESP_ERR_INVALID_STATE;
    }

    memcpy(info, &s_usb_audio.device_info, sizeof(usb_audio_device_info_t));
    return ESP_OK;
}

size_t usb_audio_host_read(uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (!s_usb_audio.initialized || !data || len == 0) return 0;

    // Wait for data with timeout
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout = pdMS_TO_TICKS(timeout_ms);

    while (ring_buffer_available() < len) {
        if ((xTaskGetTickCount() - start) >= timeout) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    return ring_buffer_read(data, len);
}

size_t usb_audio_host_available(void)
{
    return ring_buffer_available();
}

bool usb_audio_host_is_ready(void)
{
    return s_usb_audio.state == USB_AUDIO_STATE_STREAMING;
}
