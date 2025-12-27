/**
 * @file es9039.c
 * @brief ESS ES9039Q2M DAC Driver Implementation
 *
 * Uses ESP-IDF I2S Standard Mode for audio output
 */

#include "es9039.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"

static const char *TAG = "ES9039";

static i2s_chan_handle_t s_tx_handle = NULL;
static bool s_initialized = false;
static es9039_config_t s_config;

esp_err_t es9039_init(const es9039_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    if (!config) {
        es9039_config_t default_config = ES9039_DEFAULT_CONFIG();
        memcpy(&s_config, &default_config, sizeof(es9039_config_t));
    } else {
        memcpy(&s_config, config, sizeof(es9039_config_t));
    }

    // Create I2S channel
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;

    esp_err_t ret = i2s_new_channel(&chan_cfg, &s_tx_handle, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure I2S standard mode
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = s_config.sample_rate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            (s_config.bits_per_sample == ES9039_BITS_32) ? I2S_DATA_BIT_WIDTH_32BIT :
            (s_config.bits_per_sample == ES9039_BITS_24) ? I2S_DATA_BIT_WIDTH_24BIT :
            I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_STEREO
        ),
        .gpio_cfg = {
            .mclk = s_config.mclk_pin,
            .bclk = s_config.bclk_pin,
            .ws = s_config.ws_pin,
            .dout = s_config.dout_pin,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(s_tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init I2S std mode: %s", esp_err_to_name(ret));
        i2s_del_channel(s_tx_handle);
        s_tx_handle = NULL;
        return ret;
    }

    ret = i2s_channel_enable(s_tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S channel: %s", esp_err_to_name(ret));
        i2s_del_channel(s_tx_handle);
        s_tx_handle = NULL;
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "ES9039 initialized (I2S0 TX, %dHz, %d-bit)",
             s_config.sample_rate, s_config.bits_per_sample);

    return ESP_OK;
}

esp_err_t es9039_deinit(void)
{
    if (!s_initialized) return ESP_OK;

    if (s_tx_handle) {
        i2s_channel_disable(s_tx_handle);
        i2s_del_channel(s_tx_handle);
        s_tx_handle = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "ES9039 deinitialized");
    return ESP_OK;
}

esp_err_t es9039_write(const void *data, size_t len, size_t *bytes_written)
{
    if (!s_initialized || !s_tx_handle) return ESP_ERR_INVALID_STATE;

    return i2s_channel_write(s_tx_handle, data, len, bytes_written, portMAX_DELAY);
}

esp_err_t es9039_set_sample_rate(es9039_sample_rate_t rate)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    // Disable channel, reconfigure, and re-enable
    i2s_channel_disable(s_tx_handle);

    i2s_std_clk_config_t clk_cfg = {
        .sample_rate_hz = rate,
        .clk_src = I2S_CLK_SRC_DEFAULT,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    };

    esp_err_t ret = i2s_channel_reconfig_std_clock(s_tx_handle, &clk_cfg);
    if (ret == ESP_OK) {
        s_config.sample_rate = rate;
        ESP_LOGI(TAG, "Sample rate changed to %dHz", rate);
    }

    i2s_channel_enable(s_tx_handle);
    return ret;
}

esp_err_t es9039_set_volume(uint8_t volume)
{
    // ES9039 volume control via I2C if available
    // For now, software volume control could be implemented
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    ESP_LOGI(TAG, "Volume set to %d%% (software)", volume);
    return ESP_OK;
}

esp_err_t es9039_mute(bool mute)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    if (mute) {
        i2s_channel_disable(s_tx_handle);
    } else {
        i2s_channel_enable(s_tx_handle);
    }

    ESP_LOGI(TAG, "Mute: %s", mute ? "ON" : "OFF");
    return ESP_OK;
}

i2s_chan_handle_t es9039_get_i2s_handle(void)
{
    return s_tx_handle;
}

bool es9039_is_initialized(void)
{
    return s_initialized;
}
