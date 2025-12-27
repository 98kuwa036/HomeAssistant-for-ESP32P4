/**
 * @file xvf3800.c
 * @brief XMOS XVF3800 Voice Processor Driver Implementation
 *
 * Uses ESP-IDF I2S Standard Mode for audio input
 */

#include "xvf3800.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"

static const char *TAG = "XVF3800";

static i2s_chan_handle_t s_rx_handle = NULL;
static bool s_initialized = false;
static xvf3800_config_t s_config;
static xvf3800_audio_cb_t s_callback = NULL;
static void *s_callback_data = NULL;

esp_err_t xvf3800_init(const xvf3800_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    if (!config) {
        xvf3800_config_t default_config = XVF3800_DEFAULT_CONFIG();
        memcpy(&s_config, &default_config, sizeof(xvf3800_config_t));
    } else {
        memcpy(&s_config, config, sizeof(xvf3800_config_t));
    }

    // Create I2S channel (I2S1 for RX)
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);

    esp_err_t ret = i2s_new_channel(&chan_cfg, NULL, &s_rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure I2S standard mode for RX
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = s_config.sample_rate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_MONO
        ),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,  // XVF3800 provides its own clock
            .bclk = s_config.bclk_pin,
            .ws = s_config.ws_pin,
            .dout = I2S_GPIO_UNUSED,
            .din = s_config.din_pin,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(s_rx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init I2S std mode: %s", esp_err_to_name(ret));
        i2s_del_channel(s_rx_handle);
        s_rx_handle = NULL;
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "XVF3800 initialized (I2S1 RX, %luHz, %d-bit)",
             s_config.sample_rate, s_config.bits_per_sample);

    return ESP_OK;
}

esp_err_t xvf3800_deinit(void)
{
    if (!s_initialized) return ESP_OK;

    if (s_rx_handle) {
        i2s_channel_disable(s_rx_handle);
        i2s_del_channel(s_rx_handle);
        s_rx_handle = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "XVF3800 deinitialized");
    return ESP_OK;
}

esp_err_t xvf3800_start(void)
{
    if (!s_initialized || !s_rx_handle) return ESP_ERR_INVALID_STATE;

    esp_err_t ret = i2s_channel_enable(s_rx_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Audio capture started");
    }
    return ret;
}

esp_err_t xvf3800_stop(void)
{
    if (!s_initialized || !s_rx_handle) return ESP_ERR_INVALID_STATE;

    esp_err_t ret = i2s_channel_disable(s_rx_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Audio capture stopped");
    }
    return ret;
}

esp_err_t xvf3800_read(void *data, size_t len, size_t *bytes_read, uint32_t timeout_ms)
{
    if (!s_initialized || !s_rx_handle) return ESP_ERR_INVALID_STATE;

    esp_err_t ret = i2s_channel_read(s_rx_handle, data, len, bytes_read,
                                      pdMS_TO_TICKS(timeout_ms));

    // Call callback if registered
    if (ret == ESP_OK && s_callback && *bytes_read > 0) {
        size_t num_samples = *bytes_read / sizeof(int16_t);
        s_callback((const int16_t *)data, num_samples, s_callback_data);
    }

    return ret;
}

esp_err_t xvf3800_register_callback(xvf3800_audio_cb_t callback, void *user_data)
{
    s_callback = callback;
    s_callback_data = user_data;
    return ESP_OK;
}

i2s_chan_handle_t xvf3800_get_i2s_handle(void)
{
    return s_rx_handle;
}

bool xvf3800_is_initialized(void)
{
    return s_initialized;
}
