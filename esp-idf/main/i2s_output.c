/**
 * I2S Output Handler for ES8311 DAC
 *
 * Uses ESP-IDF I2S driver for audio output
 */

#include "i2s_output.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "driver/i2c.h"

static const char *TAG = "I2S_OUTPUT";

// I2S channel handle
static i2s_chan_handle_t tx_chan = NULL;

// ES8311 I2C address
#define ES8311_ADDR         0x18

// I2C configuration
#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_MASTER_FREQ_HZ  100000

// Current volume (0-100)
static uint8_t current_volume = 80;
static bool is_muted = false;

/**
 * Initialize ES8311 codec via I2C
 */
static esp_err_t es8311_init(void)
{
    ESP_LOGI(TAG, "Initializing ES8311 codec");

    // ES8311 initialization sequence
    // Note: Full initialization requires multiple register writes
    // This is a simplified version

    uint8_t data[2];

    // Reset
    data[0] = 0x00;  // Reset register
    data[1] = 0x1F;  // Reset value
    i2c_master_write_to_device(I2C_MASTER_NUM, ES8311_ADDR, data, 2, pdMS_TO_TICKS(100));

    vTaskDelay(pdMS_TO_TICKS(50));

    // Release reset
    data[0] = 0x00;
    data[1] = 0x00;
    i2c_master_write_to_device(I2C_MASTER_NUM, ES8311_ADDR, data, 2, pdMS_TO_TICKS(100));

    // Configure clock
    data[0] = 0x01;  // CLK Manager 1
    data[1] = 0x3F;  // MCLK from external
    i2c_master_write_to_device(I2C_MASTER_NUM, ES8311_ADDR, data, 2, pdMS_TO_TICKS(100));

    // Configure DAC
    data[0] = 0x32;  // DAC Control
    data[1] = 0x00;  // Normal operation
    i2c_master_write_to_device(I2C_MASTER_NUM, ES8311_ADDR, data, 2, pdMS_TO_TICKS(100));

    // Set volume
    data[0] = 0x32;  // DAC Volume
    data[1] = 0xBF;  // -0.5dB (near max)
    i2c_master_write_to_device(I2C_MASTER_NUM, ES8311_ADDR, data, 2, pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "ES8311 initialized");
    return ESP_OK;
}

esp_err_t i2s_output_init(const i2s_output_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing I2S output:");
    ESP_LOGI(TAG, "  MCLK: GPIO%d", config->mclk_pin);
    ESP_LOGI(TAG, "  BCLK: GPIO%d", config->bclk_pin);
    ESP_LOGI(TAG, "  WS: GPIO%d", config->ws_pin);
    ESP_LOGI(TAG, "  DOUT: GPIO%d", config->dout_pin);
    ESP_LOGI(TAG, "  Sample rate: %lu Hz", config->sample_rate);

    // I2S channel configuration
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;

    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // I2S standard mode configuration
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(config->sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            config->bits_per_sample == 16 ? I2S_DATA_BIT_WIDTH_16BIT : I2S_DATA_BIT_WIDTH_32BIT,
            I2S_SLOT_MODE_MONO
        ),
        .gpio_cfg = {
            .mclk = config->mclk_pin,
            .bclk = config->bclk_pin,
            .ws = config->ws_pin,
            .dout = config->dout_pin,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(tx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S standard mode: %s", esp_err_to_name(ret));
        i2s_del_channel(tx_chan);
        return ret;
    }

    ret = i2s_channel_enable(tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S channel: %s", esp_err_to_name(ret));
        i2s_del_channel(tx_chan);
        return ret;
    }

    // Initialize ES8311 codec
    ret = es8311_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ES8311 initialization failed (may not be present)");
        // Continue anyway - PAM8403 might work without ES8311 I2C control
    }

    ESP_LOGI(TAG, "I2S output initialized successfully");
    return ESP_OK;
}

void i2s_output_deinit(void)
{
    if (tx_chan != NULL) {
        i2s_channel_disable(tx_chan);
        i2s_del_channel(tx_chan);
        tx_chan = NULL;
    }

    ESP_LOGI(TAG, "I2S output deinitialized");
}

esp_err_t i2s_output_write(const uint8_t *data, size_t length, size_t *bytes_written)
{
    if (tx_chan == NULL || data == NULL || bytes_written == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (is_muted) {
        *bytes_written = length;  // Pretend we wrote it
        return ESP_OK;
    }

    esp_err_t ret = i2s_channel_write(tx_chan, data, length, bytes_written, pdMS_TO_TICKS(1000));
    return ret;
}

esp_err_t i2s_output_set_volume(uint8_t volume)
{
    if (volume > 100) {
        volume = 100;
    }

    current_volume = volume;

    // Convert 0-100 to ES8311 register value (0x00 = mute, 0xBF = max)
    uint8_t reg_value = (uint8_t)((volume * 0xBF) / 100);

    uint8_t data[2] = {0x32, reg_value};
    esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_NUM, ES8311_ADDR, data, 2, pdMS_TO_TICKS(100));

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Volume set to %d%%", volume);
    }

    return ret;
}

esp_err_t i2s_output_set_mute(bool mute)
{
    is_muted = mute;

    // Mute via ES8311 register
    uint8_t data[2] = {0x31, mute ? 0x40 : 0x00};
    esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_NUM, ES8311_ADDR, data, 2, pdMS_TO_TICKS(100));

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Mute: %s", mute ? "ON" : "OFF");
    }

    return ret;
}
