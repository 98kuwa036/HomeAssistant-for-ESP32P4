/**
 * Home Assistant Client
 *
 * Uses HTTP REST API and WebSocket for communication
 */

#include "ha_client.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"

static const char *TAG = "HA_CLIENT";

// Home Assistant configuration (should be in NVS or Kconfig)
#define HA_HOST         CONFIG_HA_HOST
#define HA_PORT         CONFIG_HA_PORT
#define HA_TOKEN        CONFIG_HA_TOKEN

#ifndef CONFIG_HA_HOST
#define CONFIG_HA_HOST "homeassistant.local"
#endif

#ifndef CONFIG_HA_PORT
#define CONFIG_HA_PORT 8123
#endif

#ifndef CONFIG_HA_TOKEN
#define CONFIG_HA_TOKEN "YOUR_LONG_LIVED_ACCESS_TOKEN"
#endif

// Connection state
static bool is_connected = false;

// Audio buffers
static RingbufHandle_t tts_ringbuf = NULL;
static const size_t TTS_BUFFER_SIZE = 65536;  // 64KB

/**
 * HTTP event handler
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP error");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP connected");
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Handle response data
                ESP_LOGD(TAG, "Received %d bytes", evt->data_len);
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

/**
 * Check connection to Home Assistant
 */
static esp_err_t check_connection(void)
{
    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d/api/", CONFIG_HA_HOST, CONFIG_HA_PORT);

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Set authorization header
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", CONFIG_HA_TOKEN);
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status == 200 || status == 201) {
            ESP_LOGI(TAG, "Connected to Home Assistant");
            is_connected = true;
        } else {
            ESP_LOGW(TAG, "HA returned status %d", status);
            is_connected = false;
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        is_connected = false;
    }

    esp_http_client_cleanup(client);
    return is_connected ? ESP_OK : ESP_FAIL;
}

esp_err_t ha_client_init(void)
{
    ESP_LOGI(TAG, "Initializing Home Assistant client");
    ESP_LOGI(TAG, "  Host: %s:%d", CONFIG_HA_HOST, CONFIG_HA_PORT);

    // Create TTS ring buffer
    tts_ringbuf = xRingbufferCreate(TTS_BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
    if (tts_ringbuf == NULL) {
        ESP_LOGE(TAG, "Failed to create TTS buffer");
        return ESP_ERR_NO_MEM;
    }

    // Check connection
    esp_err_t ret = check_connection();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Initial connection failed, will retry...");
    }

    return ESP_OK;
}

void ha_client_deinit(void)
{
    if (tts_ringbuf != NULL) {
        vRingbufferDelete(tts_ringbuf);
        tts_ringbuf = NULL;
    }
    is_connected = false;
    ESP_LOGI(TAG, "HA client deinitialized");
}

void ha_client_process(void)
{
    // Reconnect if disconnected
    if (!is_connected) {
        static uint32_t last_retry = 0;
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        if (now - last_retry > 10000) {  // Retry every 10 seconds
            last_retry = now;
            check_connection();
        }
    }

    // TODO: Handle WebSocket messages for real-time communication
    // TODO: Process voice pipeline events
}

esp_err_t ha_client_send_audio(const uint8_t *audio_data, size_t length)
{
    if (!is_connected || audio_data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // TODO: Implement voice pipeline audio streaming
    // This requires WebSocket connection to HA voice pipeline
    // For now, we'll use a simplified REST-based approach

    ESP_LOGD(TAG, "Sending %d bytes of audio data", length);

    return ESP_OK;
}

esp_err_t ha_client_receive_tts(uint8_t *buffer, size_t buffer_size, size_t *bytes_received)
{
    if (buffer == NULL || bytes_received == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t item_size;
    void *item = xRingbufferReceiveUpTo(
        tts_ringbuf,
        &item_size,
        pdMS_TO_TICKS(100),
        buffer_size
    );

    if (item != NULL) {
        memcpy(buffer, item, item_size);
        vRingbufferReturnItem(tts_ringbuf, item);
        *bytes_received = item_size;
        return ESP_OK;
    }

    *bytes_received = 0;
    return ESP_OK;
}

esp_err_t ha_client_send_sensor(const char *entity_id, float value)
{
    if (!is_connected || entity_id == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/api/states/%s",
             CONFIG_HA_HOST, CONFIG_HA_PORT, entity_id);

    // Create JSON payload
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "state", "");
    cJSON *attrs = cJSON_AddObjectToObject(root, "attributes");
    cJSON_AddNumberToObject(attrs, "value", value);

    char state_str[32];
    snprintf(state_str, sizeof(state_str), "%.2f", value);
    cJSON_ReplaceItemInObject(root, "state", cJSON_CreateString(state_str));

    char *json_str = cJSON_Print(root);

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", CONFIG_HA_TOKEN);
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, json_str, strlen(json_str));

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGD(TAG, "Sensor %s updated to %.2f", entity_id, value);
    } else {
        ESP_LOGE(TAG, "Failed to update sensor: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(json_str);

    return err;
}

bool ha_client_is_connected(void)
{
    return is_connected;
}
