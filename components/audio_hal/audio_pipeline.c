/**
 * @file audio_pipeline.c
 * @brief Dual I2S Audio Pipeline Implementation
 */

#include "audio_pipeline.h"
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"

static const char *TAG = "audio_pipe";

// ============================================================================
// Internal State
// ============================================================================

typedef struct {
    // I2S handles
    i2s_chan_handle_t i2s0_tx_handle;   // Output to DAC
    i2s_chan_handle_t i2s1_rx_handle;   // Input from Mic

    // State
    audio_state_t state;
    bool initialized;
    EventGroupHandle_t event_group;
    SemaphoreHandle_t mutex;

    // Volume
    uint8_t volume;
    bool muted;

    // Voice activity
    voice_activity_t vad;
    float energy_threshold;
    uint32_t voice_start_time;

    // Statistics
    uint32_t underruns;
    uint32_t overruns;

    // Buffers (in PSRAM)
    uint8_t *output_buffer;
    uint8_t *input_buffer;
    size_t output_write_pos;
    size_t output_read_pos;
    size_t input_write_pos;
    size_t input_read_pos;

} audio_pipeline_state_t;

static audio_pipeline_state_t s_audio = {0};

// Event bits
#define AUDIO_READY_BIT     BIT0
#define AUDIO_PLAYING_BIT   BIT1
#define AUDIO_RECORDING_BIT BIT2

// ============================================================================
// I2S Configuration
// ============================================================================

static esp_err_t init_i2s0_output(void)
{
    ESP_LOGI(TAG, "Initializing I2S0 (DAC output)...");

    // I2S0 channel config (TX only)
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = AUDIO_DMA_DESC_NUM;
    chan_cfg.dma_frame_num = AUDIO_DMA_FRAME_NUM;
    chan_cfg.auto_clear = true;

    ESP_RETURN_ON_ERROR(
        i2s_new_channel(&chan_cfg, &s_audio.i2s0_tx_handle, NULL),
        TAG, "Failed to create I2S0 TX channel"
    );

    // Standard I2S config for DAC
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(CONFIG_I2S0_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            (i2s_data_bit_width_t)CONFIG_I2S0_BIT_WIDTH,
            I2S_SLOT_MODE_STEREO
        ),
        .gpio_cfg = {
            .mclk = CONFIG_I2S0_MCLK_GPIO,
            .bclk = CONFIG_I2S0_BCK_GPIO,
            .ws = CONFIG_I2S0_WS_GPIO,
            .dout = CONFIG_I2S0_DOUT_GPIO,
            .din = GPIO_NUM_NC,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    // Set MCLK multiplier for ES9039Q2M (needs 256fs or 512fs)
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

    ESP_RETURN_ON_ERROR(
        i2s_channel_init_std_mode(s_audio.i2s0_tx_handle, &std_cfg),
        TAG, "Failed to init I2S0 STD mode"
    );

    ESP_LOGI(TAG, "I2S0 initialized: %d Hz, %d-bit, MCLK=GPIO%d, BCK=GPIO%d, WS=GPIO%d, DOUT=GPIO%d",
             CONFIG_I2S0_SAMPLE_RATE, CONFIG_I2S0_BIT_WIDTH,
             CONFIG_I2S0_MCLK_GPIO, CONFIG_I2S0_BCK_GPIO,
             CONFIG_I2S0_WS_GPIO, CONFIG_I2S0_DOUT_GPIO);

    return ESP_OK;
}

static esp_err_t init_i2s1_input(void)
{
    ESP_LOGI(TAG, "Initializing I2S1 (Mic input)...");

    // I2S1 channel config (RX only, slave mode)
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_SLAVE);
    chan_cfg.dma_desc_num = AUDIO_DMA_DESC_NUM;
    chan_cfg.dma_frame_num = AUDIO_DMA_FRAME_NUM;

    ESP_RETURN_ON_ERROR(
        i2s_new_channel(&chan_cfg, NULL, &s_audio.i2s1_rx_handle),
        TAG, "Failed to create I2S1 RX channel"
    );

    // Standard I2S config for ReSpeaker
    // ReSpeaker XVF3800 outputs 16kHz 16-bit stereo after processing
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(CONFIG_I2S1_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_STEREO
        ),
        .gpio_cfg = {
            .mclk = CONFIG_I2S1_MCLK_GPIO,
            .bclk = CONFIG_I2S1_BCK_GPIO,
            .ws = CONFIG_I2S1_WS_GPIO,
            .dout = GPIO_NUM_NC,
            .din = CONFIG_I2S1_DIN_GPIO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_RETURN_ON_ERROR(
        i2s_channel_init_std_mode(s_audio.i2s1_rx_handle, &std_cfg),
        TAG, "Failed to init I2S1 STD mode"
    );

    ESP_LOGI(TAG, "I2S1 initialized: %d Hz, 16-bit, BCK=GPIO%d, WS=GPIO%d, DIN=GPIO%d",
             CONFIG_I2S1_SAMPLE_RATE,
             CONFIG_I2S1_BCK_GPIO, CONFIG_I2S1_WS_GPIO, CONFIG_I2S1_DIN_GPIO);

    return ESP_OK;
}

// ============================================================================
// Voice Activity Detection
// ============================================================================

/**
 * @brief Calculate audio energy in dB
 */
static float calculate_energy_db(const int16_t *samples, size_t num_samples)
{
    if (num_samples == 0) return -96.0f;

    int64_t sum_squares = 0;
    for (size_t i = 0; i < num_samples; i++) {
        int32_t sample = samples[i];
        sum_squares += sample * sample;
    }

    float rms = sqrtf((float)sum_squares / num_samples);
    if (rms < 1.0f) rms = 1.0f;

    // Convert to dB (relative to full scale 32767)
    float db = 20.0f * log10f(rms / 32767.0f);
    return db;
}

/**
 * @brief Update voice activity detection state
 */
static void update_vad(const int16_t *samples, size_t num_samples)
{
    float energy = calculate_energy_db(samples, num_samples);
    s_audio.vad.energy_db = energy;

    bool was_active = s_audio.vad.is_active;
    s_audio.vad.is_active = (energy > s_audio.energy_threshold);

    if (s_audio.vad.is_active && !was_active) {
        // Voice started
        s_audio.voice_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        s_audio.vad.duration_ms = 0;
    } else if (s_audio.vad.is_active) {
        // Voice ongoing
        s_audio.vad.duration_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) - s_audio.voice_start_time;
    }
}

// ============================================================================
// Public API Implementation
// ============================================================================

esp_err_t audio_pipeline_init(void)
{
    if (s_audio.initialized) {
        ESP_LOGW(TAG, "Audio pipeline already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing audio pipeline...");

    // Create synchronization primitives
    s_audio.event_group = xEventGroupCreate();
    s_audio.mutex = xSemaphoreCreateMutex();
    if (!s_audio.event_group || !s_audio.mutex) {
        return ESP_ERR_NO_MEM;
    }

    // Allocate buffers in PSRAM
    s_audio.output_buffer = heap_caps_malloc(AUDIO_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    s_audio.input_buffer = heap_caps_malloc(AUDIO_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    if (!s_audio.output_buffer || !s_audio.input_buffer) {
        ESP_LOGE(TAG, "Failed to allocate audio buffers");
        return ESP_ERR_NO_MEM;
    }

    // Initialize I2S channels
    esp_err_t ret = init_i2s0_output();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S0 init failed");
        return ret;
    }

    ret = init_i2s1_input();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S1 init failed");
        return ret;
    }

    // Set defaults
    s_audio.volume = 70;
    s_audio.muted = false;
    s_audio.energy_threshold = -40.0f;  // dB threshold for VAD
    s_audio.state = AUDIO_STATE_IDLE;
    s_audio.initialized = true;

    // Enable channels
    i2s_channel_enable(s_audio.i2s0_tx_handle);
    i2s_channel_enable(s_audio.i2s1_rx_handle);

    xEventGroupSetBits(s_audio.event_group, AUDIO_READY_BIT);

    ESP_LOGI(TAG, "Audio pipeline initialized successfully");
    return ESP_OK;
}

void audio_pipeline_deinit(void)
{
    if (!s_audio.initialized) return;

    // Disable and delete channels
    if (s_audio.i2s0_tx_handle) {
        i2s_channel_disable(s_audio.i2s0_tx_handle);
        i2s_del_channel(s_audio.i2s0_tx_handle);
    }

    if (s_audio.i2s1_rx_handle) {
        i2s_channel_disable(s_audio.i2s1_rx_handle);
        i2s_del_channel(s_audio.i2s1_rx_handle);
    }

    // Free buffers
    if (s_audio.output_buffer) {
        free(s_audio.output_buffer);
    }
    if (s_audio.input_buffer) {
        free(s_audio.input_buffer);
    }

    // Delete primitives
    if (s_audio.event_group) {
        vEventGroupDelete(s_audio.event_group);
    }
    if (s_audio.mutex) {
        vSemaphoreDelete(s_audio.mutex);
    }

    memset(&s_audio, 0, sizeof(s_audio));
}

esp_err_t audio_pipeline_wait_ready(uint32_t timeout_ms)
{
    if (!s_audio.event_group) return ESP_ERR_INVALID_STATE;

    EventBits_t bits = xEventGroupWaitBits(
        s_audio.event_group,
        AUDIO_READY_BIT,
        pdFALSE,
        pdTRUE,
        pdMS_TO_TICKS(timeout_ms)
    );

    return (bits & AUDIO_READY_BIT) ? ESP_OK : ESP_ERR_TIMEOUT;
}

void audio_pipeline_process(void)
{
    if (!s_audio.initialized) return;

    // Read from mic (I2S1)
    uint8_t rx_buf[512];
    size_t bytes_read = 0;

    esp_err_t ret = i2s_channel_read(s_audio.i2s1_rx_handle, rx_buf, sizeof(rx_buf),
                                      &bytes_read, pdMS_TO_TICKS(10));

    if (ret == ESP_OK && bytes_read > 0) {
        // Update VAD
        update_vad((int16_t *)rx_buf, bytes_read / 2);

        // Store in input buffer (ring buffer logic)
        if (xSemaphoreTake(s_audio.mutex, pdMS_TO_TICKS(10))) {
            size_t space = AUDIO_BUFFER_SIZE - s_audio.input_write_pos;
            if (bytes_read <= space) {
                memcpy(s_audio.input_buffer + s_audio.input_write_pos, rx_buf, bytes_read);
                s_audio.input_write_pos = (s_audio.input_write_pos + bytes_read) % AUDIO_BUFFER_SIZE;
            } else {
                s_audio.overruns++;
            }
            xSemaphoreGive(s_audio.mutex);
        }
    }

    // Write to DAC (I2S0) if playing
    if (s_audio.state == AUDIO_STATE_PLAYING || s_audio.state == AUDIO_STATE_DUPLEX) {
        if (xSemaphoreTake(s_audio.mutex, pdMS_TO_TICKS(10))) {
            size_t available = (s_audio.output_write_pos - s_audio.output_read_pos + AUDIO_BUFFER_SIZE) % AUDIO_BUFFER_SIZE;
            if (available >= 512) {
                uint8_t tx_buf[512];
                size_t to_send = 512;

                // Copy from ring buffer
                if (s_audio.output_read_pos + to_send <= AUDIO_BUFFER_SIZE) {
                    memcpy(tx_buf, s_audio.output_buffer + s_audio.output_read_pos, to_send);
                } else {
                    size_t first = AUDIO_BUFFER_SIZE - s_audio.output_read_pos;
                    memcpy(tx_buf, s_audio.output_buffer + s_audio.output_read_pos, first);
                    memcpy(tx_buf + first, s_audio.output_buffer, to_send - first);
                }
                s_audio.output_read_pos = (s_audio.output_read_pos + to_send) % AUDIO_BUFFER_SIZE;

                xSemaphoreGive(s_audio.mutex);

                // Apply volume
                if (!s_audio.muted) {
                    int16_t *samples = (int16_t *)tx_buf;
                    float vol_scale = s_audio.volume / 100.0f;
                    for (size_t i = 0; i < to_send / 2; i++) {
                        samples[i] = (int16_t)(samples[i] * vol_scale);
                    }
                } else {
                    memset(tx_buf, 0, to_send);
                }

                size_t bytes_written;
                i2s_channel_write(s_audio.i2s0_tx_handle, tx_buf, to_send, &bytes_written, pdMS_TO_TICKS(10));
            } else {
                xSemaphoreGive(s_audio.mutex);
                s_audio.underruns++;
            }
        }
    }
}

audio_state_t audio_pipeline_get_state(void)
{
    return s_audio.state;
}

esp_err_t audio_pipeline_play(void)
{
    s_audio.state = AUDIO_STATE_PLAYING;
    xEventGroupSetBits(s_audio.event_group, AUDIO_PLAYING_BIT);
    return ESP_OK;
}

esp_err_t audio_pipeline_stop(void)
{
    s_audio.state = AUDIO_STATE_IDLE;
    xEventGroupClearBits(s_audio.event_group, AUDIO_PLAYING_BIT | AUDIO_RECORDING_BIT);

    // Clear buffers
    if (xSemaphoreTake(s_audio.mutex, pdMS_TO_TICKS(100))) {
        s_audio.output_write_pos = 0;
        s_audio.output_read_pos = 0;
        xSemaphoreGive(s_audio.mutex);
    }

    return ESP_OK;
}

esp_err_t audio_pipeline_pause(void)
{
    if (s_audio.state == AUDIO_STATE_PLAYING) {
        s_audio.state = AUDIO_STATE_IDLE;
        xEventGroupClearBits(s_audio.event_group, AUDIO_PLAYING_BIT);
    }
    return ESP_OK;
}

esp_err_t audio_pipeline_resume(void)
{
    return audio_pipeline_play();
}

size_t audio_pipeline_write(const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (!s_audio.initialized || !data || len == 0) return 0;

    if (xSemaphoreTake(s_audio.mutex, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return 0;
    }

    size_t space = (s_audio.output_read_pos - s_audio.output_write_pos - 1 + AUDIO_BUFFER_SIZE) % AUDIO_BUFFER_SIZE;
    size_t to_write = (len < space) ? len : space;

    if (to_write > 0) {
        size_t first_chunk = AUDIO_BUFFER_SIZE - s_audio.output_write_pos;
        if (to_write <= first_chunk) {
            memcpy(s_audio.output_buffer + s_audio.output_write_pos, data, to_write);
        } else {
            memcpy(s_audio.output_buffer + s_audio.output_write_pos, data, first_chunk);
            memcpy(s_audio.output_buffer, data + first_chunk, to_write - first_chunk);
        }
        s_audio.output_write_pos = (s_audio.output_write_pos + to_write) % AUDIO_BUFFER_SIZE;
    }

    xSemaphoreGive(s_audio.mutex);
    return to_write;
}

esp_err_t audio_pipeline_record_start(void)
{
    s_audio.state = AUDIO_STATE_RECORDING;
    xEventGroupSetBits(s_audio.event_group, AUDIO_RECORDING_BIT);
    return ESP_OK;
}

esp_err_t audio_pipeline_record_stop(void)
{
    s_audio.state = AUDIO_STATE_IDLE;
    xEventGroupClearBits(s_audio.event_group, AUDIO_RECORDING_BIT);
    return ESP_OK;
}

size_t audio_pipeline_read(uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (!s_audio.initialized || !data || len == 0) return 0;

    if (xSemaphoreTake(s_audio.mutex, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return 0;
    }

    size_t available = (s_audio.input_write_pos - s_audio.input_read_pos + AUDIO_BUFFER_SIZE) % AUDIO_BUFFER_SIZE;
    size_t to_read = (len < available) ? len : available;

    if (to_read > 0) {
        size_t first_chunk = AUDIO_BUFFER_SIZE - s_audio.input_read_pos;
        if (to_read <= first_chunk) {
            memcpy(data, s_audio.input_buffer + s_audio.input_read_pos, to_read);
        } else {
            memcpy(data, s_audio.input_buffer + s_audio.input_read_pos, first_chunk);
            memcpy(data + first_chunk, s_audio.input_buffer, to_read - first_chunk);
        }
        s_audio.input_read_pos = (s_audio.input_read_pos + to_read) % AUDIO_BUFFER_SIZE;
    }

    xSemaphoreGive(s_audio.mutex);
    return to_read;
}

bool audio_pipeline_voice_detected(void)
{
    return s_audio.vad.is_active;
}

void audio_pipeline_get_voice_activity(voice_activity_t *activity)
{
    if (activity) {
        memcpy(activity, &s_audio.vad, sizeof(voice_activity_t));
    }
}

esp_err_t audio_pipeline_set_volume(uint8_t volume)
{
    if (volume > 100) volume = 100;
    s_audio.volume = volume;
    ESP_LOGI(TAG, "Volume set to %d%%", volume);
    return ESP_OK;
}

uint8_t audio_pipeline_get_volume(void)
{
    return s_audio.volume;
}

esp_err_t audio_pipeline_set_mute(bool mute)
{
    s_audio.muted = mute;
    ESP_LOGI(TAG, "Mute: %s", mute ? "ON" : "OFF");
    return ESP_OK;
}

void audio_pipeline_get_buffer_levels(uint8_t *output_level, uint8_t *input_level)
{
    if (output_level) {
        size_t used = (s_audio.output_write_pos - s_audio.output_read_pos + AUDIO_BUFFER_SIZE) % AUDIO_BUFFER_SIZE;
        *output_level = (used * 100) / AUDIO_BUFFER_SIZE;
    }
    if (input_level) {
        size_t used = (s_audio.input_write_pos - s_audio.input_read_pos + AUDIO_BUFFER_SIZE) % AUDIO_BUFFER_SIZE;
        *input_level = (used * 100) / AUDIO_BUFFER_SIZE;
    }
}

void audio_pipeline_get_stats(uint32_t *underruns, uint32_t *overruns)
{
    if (underruns) *underruns = s_audio.underruns;
    if (overruns) *overruns = s_audio.overruns;
}
