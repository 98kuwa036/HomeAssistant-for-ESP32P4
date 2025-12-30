/**
 * @file audio_pipeline.c
 * @brief Audio Pipeline Implementation with I2S/USB Mic + I2S DAC
 *
 * Audio Flow:
 *   [USB Audio] ReSpeaker USB (48kHz) --> usb_host_uac --> Dual Buffers
 *       OR
 *   [I2S Audio] I2S1 Microphone (48kHz) --> I2S RX --> Dual Buffers
 *                                                        |
 *                                       ┌────────────────┴────────────────┐
 *                                       |                                 |
 *                                 RAW (48kHz)                    Processed (16kHz)
 *                                 Local LLM                      ESPHome/HA
 *
 *   Output Buffer --> I2S0 --> ES9038Q2M DAC --> Peerless Speaker
 *
 * Supported Microphones:
 *   USB (CONFIG_AUDIO_INPUT_USB):
 *     - ReSpeaker USB Mic Array v2.0 (with beamforming)
 *     - Any UAC 1.0 compatible USB microphone
 *
 *   I2S (CONFIG_AUDIO_INPUT_I2S):
 *     - INMP441: I2S MEMS microphone (mono)
 *     - SPH0645: I2S MEMS microphone (mono)
 *     - ICS-43434: I2S MEMS microphone (stereo)
 */

#include "audio_pipeline.h"
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"

// Include USB Audio Input when enabled
#ifdef CONFIG_AUDIO_INPUT_USB
#include "usb_audio_input.h"
#endif

static const char *TAG = "audio_pipe";

// ============================================================================
// Audio Processing Configuration
// ============================================================================

// Downsampling ratio: 48kHz -> 16kHz = 3:1
#ifndef CONFIG_MIC_SAMPLE_RATE
#define CONFIG_MIC_SAMPLE_RATE    48000
#endif
#ifndef CONFIG_PROCESSED_SAMPLE_RATE
#define CONFIG_PROCESSED_SAMPLE_RATE    16000
#endif
#define DOWNSAMPLE_RATIO    (CONFIG_MIC_SAMPLE_RATE / CONFIG_PROCESSED_SAMPLE_RATE)

// Input channels (1=mono for INMP441, 2=stereo for ICS-43434)
#ifndef CONFIG_MIC_CHANNELS
#define CONFIG_MIC_CHANNELS     1
#endif
#define INPUT_CHANNELS      CONFIG_MIC_CHANNELS

// I2S1 Microphone DMA configuration
#define MIC_DMA_DESC_NUM    6
#define MIC_DMA_FRAME_NUM   240     // ~5ms at 48kHz

// ============================================================================
// Buffer Size Configuration
// ============================================================================

// Raw buffer: 48kHz stereo (high quality for local LLM)
#define RAW_BUFFER_SIZE         (64 * 1024)   // 64KB (~170ms at 48kHz stereo)

// Processed buffer: 16kHz mono (for ESPHome)
#define PROCESSED_BUFFER_SIZE   (16 * 1024)   // 16KB (~500ms at 16kHz mono)

// ============================================================================
// Internal State
// ============================================================================

typedef struct {
    // I2S handles
    i2s_chan_handle_t i2s0_tx_handle;   // Output to ES9038Q2M DAC
    i2s_chan_handle_t i2s1_rx_handle;   // Input from I2S microphone

    // Microphone state
    bool mic_ready;
    bool mic_streaming;
    TaskHandle_t mic_task_handle;

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
    uint32_t raw_overruns;

    // Output buffer (to DAC)
    uint8_t *output_buffer;
    size_t output_write_pos;
    size_t output_read_pos;

    // =========================================
    // Dual Input Buffers (for different use cases)
    // =========================================

    // Raw buffer: 48kHz stereo (high quality)
    // Use case: Local LLM (Qwen2.5), high-quality recording
    uint8_t *input_buffer_raw;
    size_t raw_write_pos;
    size_t raw_read_pos;

    // Processed buffer: 16kHz mono (ESPHome compatible)
    // Use case: ESPHome voice assistant, cloud STT
    uint8_t *input_buffer_processed;
    size_t processed_write_pos;
    size_t processed_read_pos;

    // Legacy alias (points to processed buffer for compatibility)
    uint8_t *input_buffer;
    size_t input_write_pos;
    size_t input_read_pos;

    // Downsampling state
    uint32_t downsample_counter;

} audio_pipeline_state_t;

static audio_pipeline_state_t s_audio = {0};

// Event bits
#define AUDIO_READY_BIT     BIT0
#define AUDIO_PLAYING_BIT   BIT1
#define AUDIO_RECORDING_BIT BIT2

// Forward declarations
static size_t stereo_to_mono_downsample(const int16_t *input, int16_t *output, size_t in_samples);
static size_t mono_downsample(const int16_t *input, int16_t *output, size_t in_samples);
static void update_vad(const int16_t *samples, size_t num_samples);

// ============================================================================
// Audio Data Processing (shared by both USB and I2S input)
// ============================================================================

/**
 * @brief Process incoming audio data from microphone (USB or I2S)
 *
 * Stores raw data to RAW buffer and processed (downsampled) data to Processed buffer.
 */
static void process_mic_data(const uint8_t *data, size_t len)
{
    if (!s_audio.initialized || len == 0) return;

    size_t samples_per_frame = (INPUT_CHANNELS == 2) ? 2 : 1;
    size_t frames = len / (2 * samples_per_frame);  // 2 bytes per sample

    if (xSemaphoreTake(s_audio.mutex, pdMS_TO_TICKS(5))) {
        // ========================================
        // 1. Store raw audio data (high quality)
        // ========================================
        size_t raw_space = (s_audio.raw_read_pos - s_audio.raw_write_pos - 1 + RAW_BUFFER_SIZE) % RAW_BUFFER_SIZE;
        if (len <= raw_space) {
            size_t first_chunk = RAW_BUFFER_SIZE - s_audio.raw_write_pos;
            if (len <= first_chunk) {
                memcpy(s_audio.input_buffer_raw + s_audio.raw_write_pos, data, len);
            } else {
                memcpy(s_audio.input_buffer_raw + s_audio.raw_write_pos, data, first_chunk);
                memcpy(s_audio.input_buffer_raw, data + first_chunk, len - first_chunk);
            }
            s_audio.raw_write_pos = (s_audio.raw_write_pos + len) % RAW_BUFFER_SIZE;
        } else {
            s_audio.raw_overruns++;
        }

        // ========================================
        // 2. Convert & store 16kHz mono (ESPHome compatible)
        // ========================================
        int16_t rx_buf_mono[frames / DOWNSAMPLE_RATIO + 1];
        size_t mono_samples;

        if (INPUT_CHANNELS == 2) {
            // Stereo input: mix to mono + downsample
            mono_samples = stereo_to_mono_downsample(
                (const int16_t *)data,
                rx_buf_mono,
                frames
            );
        } else {
            // Mono input: just downsample
            mono_samples = mono_downsample(
                (const int16_t *)data,
                rx_buf_mono,
                frames
            );
        }
        size_t mono_bytes = mono_samples * sizeof(int16_t);

        size_t proc_space = (s_audio.processed_read_pos - s_audio.processed_write_pos - 1 + PROCESSED_BUFFER_SIZE) % PROCESSED_BUFFER_SIZE;
        if (mono_bytes <= proc_space) {
            size_t first_chunk = PROCESSED_BUFFER_SIZE - s_audio.processed_write_pos;
            if (mono_bytes <= first_chunk) {
                memcpy(s_audio.input_buffer_processed + s_audio.processed_write_pos, rx_buf_mono, mono_bytes);
            } else {
                memcpy(s_audio.input_buffer_processed + s_audio.processed_write_pos, rx_buf_mono, first_chunk);
                memcpy(s_audio.input_buffer_processed, (uint8_t*)rx_buf_mono + first_chunk, mono_bytes - first_chunk);
            }
            s_audio.processed_write_pos = (s_audio.processed_write_pos + mono_bytes) % PROCESSED_BUFFER_SIZE;

            // Legacy alias sync
            s_audio.input_write_pos = s_audio.processed_write_pos;
        } else {
            s_audio.overruns++;
        }

        xSemaphoreGive(s_audio.mutex);

        // Update VAD with mono data
        if (mono_samples > 0) {
            update_vad(rx_buf_mono, mono_samples);
        }
    }
}

// ============================================================================
// I2S Microphone Task (only when I2S input is enabled)
// ============================================================================

#ifndef CONFIG_AUDIO_INPUT_USB
/**
 * @brief I2S Microphone reading task
 *
 * Continuously reads audio data from I2S1 microphone and processes it.
 */
static void mic_read_task(void *arg)
{
    const size_t buf_size = MIC_DMA_FRAME_NUM * INPUT_CHANNELS * sizeof(int16_t);
    uint8_t *rx_buffer = heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    if (!rx_buffer) {
        ESP_LOGE(TAG, "Failed to allocate mic RX buffer");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Microphone task started (I2S1)");
    s_audio.mic_streaming = true;

    while (s_audio.mic_ready) {
        size_t bytes_read = 0;
        esp_err_t ret = i2s_channel_read(s_audio.i2s1_rx_handle, rx_buffer, buf_size,
                                          &bytes_read, pdMS_TO_TICKS(100));

        if (ret == ESP_OK && bytes_read > 0) {
            process_mic_data(rx_buffer, bytes_read);
        } else if (ret == ESP_ERR_TIMEOUT) {
            // Timeout is OK, just continue
        } else {
            ESP_LOGW(TAG, "I2S read error: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    s_audio.mic_streaming = false;
    free(rx_buffer);
    ESP_LOGI(TAG, "Microphone task stopped");
    vTaskDelete(NULL);
}
#endif  // !CONFIG_AUDIO_INPUT_USB (I2S mic task)

// ============================================================================
// USB Audio Input Callbacks (when USB input is enabled)
// ============================================================================

#ifdef CONFIG_AUDIO_INPUT_USB

// USB Audio is always 48kHz stereo from ReSpeaker
#define USB_AUDIO_SAMPLE_RATE   48000
#define USB_AUDIO_CHANNELS      2
#define USB_DOWNSAMPLE_RATIO    (USB_AUDIO_SAMPLE_RATE / CONFIG_PROCESSED_SAMPLE_RATE)

// Separate counter for USB audio downsampling
static uint32_t s_usb_downsample_counter = 0;

/**
 * @brief Convert USB stereo 48kHz to mono 16kHz
 *
 * USB Audio from ReSpeaker is always 48kHz stereo.
 * This function converts it to 16kHz mono for Home Assistant.
 */
static size_t usb_stereo_to_mono_16k(const int16_t *input, int16_t *output, size_t stereo_frames)
{
    size_t out_idx = 0;

    for (size_t i = 0; i < stereo_frames; i++) {
        // Each stereo frame: [Left, Right]
        int16_t left = input[i * 2];
        int16_t right = input[i * 2 + 1];

        // Mix to mono (average both channels)
        int32_t mono = ((int32_t)left + (int32_t)right) / 2;

        // Downsample 48kHz -> 16kHz (take every 3rd sample)
        s_usb_downsample_counter++;
        if (s_usb_downsample_counter >= USB_DOWNSAMPLE_RATIO) {
            s_usb_downsample_counter = 0;
            output[out_idx++] = (int16_t)mono;
        }
    }

    return out_idx;
}

/**
 * @brief Process USB audio data (always 48kHz stereo)
 *
 * Stores raw stereo data and downsampled 16kHz mono data.
 */
static void process_usb_audio_data(const uint8_t *data, size_t len)
{
    if (!s_audio.initialized || len == 0) return;

    // USB audio: 48kHz, 16-bit, stereo
    size_t stereo_frames = len / (2 * USB_AUDIO_CHANNELS);  // 2 bytes per sample, 2 channels

    if (xSemaphoreTake(s_audio.mutex, pdMS_TO_TICKS(5))) {
        // ========================================
        // 1. Store raw 48kHz stereo data
        // ========================================
        size_t raw_space = (s_audio.raw_read_pos - s_audio.raw_write_pos - 1 + RAW_BUFFER_SIZE) % RAW_BUFFER_SIZE;
        if (len <= raw_space) {
            size_t first_chunk = RAW_BUFFER_SIZE - s_audio.raw_write_pos;
            if (len <= first_chunk) {
                memcpy(s_audio.input_buffer_raw + s_audio.raw_write_pos, data, len);
            } else {
                memcpy(s_audio.input_buffer_raw + s_audio.raw_write_pos, data, first_chunk);
                memcpy(s_audio.input_buffer_raw, data + first_chunk, len - first_chunk);
            }
            s_audio.raw_write_pos = (s_audio.raw_write_pos + len) % RAW_BUFFER_SIZE;
        } else {
            s_audio.raw_overruns++;
        }

        // ========================================
        // 2. Convert to 16kHz mono for Home Assistant
        // ========================================
        int16_t mono_buf[stereo_frames / USB_DOWNSAMPLE_RATIO + 1];
        size_t mono_samples = usb_stereo_to_mono_16k(
            (const int16_t *)data,
            mono_buf,
            stereo_frames
        );
        size_t mono_bytes = mono_samples * sizeof(int16_t);

        size_t proc_space = (s_audio.processed_read_pos - s_audio.processed_write_pos - 1 + PROCESSED_BUFFER_SIZE) % PROCESSED_BUFFER_SIZE;
        if (mono_bytes <= proc_space) {
            size_t first_chunk = PROCESSED_BUFFER_SIZE - s_audio.processed_write_pos;
            if (mono_bytes <= first_chunk) {
                memcpy(s_audio.input_buffer_processed + s_audio.processed_write_pos, mono_buf, mono_bytes);
            } else {
                memcpy(s_audio.input_buffer_processed + s_audio.processed_write_pos, mono_buf, first_chunk);
                memcpy(s_audio.input_buffer_processed, (uint8_t*)mono_buf + first_chunk, mono_bytes - first_chunk);
            }
            s_audio.processed_write_pos = (s_audio.processed_write_pos + mono_bytes) % PROCESSED_BUFFER_SIZE;

            // Legacy alias sync
            s_audio.input_write_pos = s_audio.processed_write_pos;
        } else {
            s_audio.overruns++;
        }

        xSemaphoreGive(s_audio.mutex);

        // Update VAD with 16kHz mono data
        if (mono_samples > 0) {
            update_vad(mono_buf, mono_samples);
        }
    }
}

/**
 * @brief USB Audio data callback - handles 48kHz stereo from ReSpeaker
 */
static void usb_audio_data_callback(const uint8_t *data, size_t len, void *user_ctx)
{
    process_usb_audio_data(data, len);
}

/**
 * @brief USB Audio connection callback
 */
static void usb_audio_connect_callback(bool connected, const usb_audio_input_info_t *info, void *user_ctx)
{
    if (connected && info) {
        ESP_LOGI(TAG, "USB Audio connected!");
        ESP_LOGI(TAG, "  VID: 0x%04X, PID: 0x%04X", info->vid, info->pid);
        ESP_LOGI(TAG, "  Format: %lu Hz, %d ch, %d-bit",
                 info->sample_rate, info->channels, info->bit_depth);
        if (info->is_respeaker) {
            ESP_LOGI(TAG, "  ReSpeaker USB Mic Array detected - Beamforming enabled!");
        }

        s_audio.mic_ready = true;

        // Auto-start streaming
        usb_audio_input_start();
        s_audio.mic_streaming = true;
    } else {
        ESP_LOGW(TAG, "USB Audio disconnected");
        s_audio.mic_ready = false;
        s_audio.mic_streaming = false;
    }
}

static esp_err_t init_usb_audio_input(void)
{
    ESP_LOGI(TAG, "Initializing USB Audio Input...");
    ESP_LOGI(TAG, "  Waiting for USB microphone connection...");
    ESP_LOGI(TAG, "  Supported: ReSpeaker USB Mic Array, UAC 1.0 devices");

    usb_audio_input_config_t config = {
        .data_cb = usb_audio_data_callback,
        .connect_cb = usb_audio_connect_callback,
        .user_ctx = NULL,
        .preferred_sample_rate = 48000,
        .preferred_channels = 2,  // Stereo for ReSpeaker
    };

    esp_err_t ret = usb_audio_input_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB Audio Input init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}
#endif  // CONFIG_AUDIO_INPUT_USB

// ============================================================================
// Audio Processing: Stereo to Mono + Downsampling
// ============================================================================

/**
 * @brief Convert stereo 16-bit samples to mono with downsampling
 *
 * Input:  Stereo 48kHz 16-bit (L, R, L, R, ...)
 * Output: Mono 16kHz 16-bit
 *
 * Process:
 * 1. Mix stereo to mono: (Left + Right) / 2
 * 2. Downsample 48kHz -> 16kHz: take every 3rd sample
 *
 * @param input     Input buffer (stereo 48kHz)
 * @param output    Output buffer (mono 16kHz)
 * @param in_samples Number of stereo sample pairs in input
 * @return Number of mono samples written to output
 */
static size_t stereo_to_mono_downsample(const int16_t *input, int16_t *output,
                                         size_t in_samples)
{
    size_t out_idx = 0;

    for (size_t i = 0; i < in_samples; i++) {
        // Each stereo frame: [Left, Right]
        int16_t left = input[i * 2];
        int16_t right = input[i * 2 + 1];

        // Mix to mono (average both channels)
        int32_t mono = ((int32_t)left + (int32_t)right) / 2;

        // Downsampling: only keep every DOWNSAMPLE_RATIO-th sample
        s_audio.downsample_counter++;
        if (s_audio.downsample_counter >= DOWNSAMPLE_RATIO) {
            s_audio.downsample_counter = 0;
            output[out_idx++] = (int16_t)mono;
        }
    }

    return out_idx;
}

/**
 * @brief Downsample mono input
 *
 * Input:  Mono 48kHz 16-bit
 * Output: Mono 16kHz 16-bit
 *
 * @param input     Input buffer (mono 48kHz)
 * @param output    Output buffer (mono 16kHz)
 * @param in_samples Number of samples in input
 * @return Number of samples written to output
 */
static size_t mono_downsample(const int16_t *input, int16_t *output, size_t in_samples)
{
    size_t out_idx = 0;

    for (size_t i = 0; i < in_samples; i++) {
        // Downsampling: only keep every DOWNSAMPLE_RATIO-th sample
        s_audio.downsample_counter++;
        if (s_audio.downsample_counter >= DOWNSAMPLE_RATIO) {
            s_audio.downsample_counter = 0;
            output[out_idx++] = input[i];
        }
    }

    return out_idx;
}

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

static esp_err_t init_i2s1_microphone(void)
{
    ESP_LOGI(TAG, "Initializing I2S1 Microphone input...");

    // I2S1 channel config (RX only)
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = MIC_DMA_DESC_NUM;
    chan_cfg.dma_frame_num = MIC_DMA_FRAME_NUM;
    chan_cfg.auto_clear = true;

    ESP_RETURN_ON_ERROR(
        i2s_new_channel(&chan_cfg, NULL, &s_audio.i2s1_rx_handle),
        TAG, "Failed to create I2S1 RX channel"
    );

    // Standard I2S config for microphone (INMP441, SPH0645, etc.)
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(CONFIG_MIC_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT,
            (INPUT_CHANNELS == 2) ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO
        ),
        .gpio_cfg = {
            .mclk = GPIO_NUM_NC,  // Most I2S mics don't need MCLK
#ifdef CONFIG_I2S1_BCK_GPIO
            .bclk = CONFIG_I2S1_BCK_GPIO,
#else
            .bclk = 15,  // Default
#endif
#ifdef CONFIG_I2S1_WS_GPIO
            .ws = CONFIG_I2S1_WS_GPIO,
#else
            .ws = 16,    // Default
#endif
            .dout = GPIO_NUM_NC,
#ifdef CONFIG_I2S1_DIN_GPIO
            .din = CONFIG_I2S1_DIN_GPIO,
#else
            .din = 17,   // Default
#endif
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    // For mono microphones like INMP441, select left or right channel
#if INPUT_CHANNELS == 1
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;  // INMP441 with L/R pin to GND
#endif

    ESP_RETURN_ON_ERROR(
        i2s_channel_init_std_mode(s_audio.i2s1_rx_handle, &std_cfg),
        TAG, "Failed to init I2S1 STD mode"
    );

    ESP_LOGI(TAG, "I2S1 Microphone initialized:");
    ESP_LOGI(TAG, "  Sample rate: %d Hz", CONFIG_MIC_SAMPLE_RATE);
    ESP_LOGI(TAG, "  Channels: %d (%s)", INPUT_CHANNELS, INPUT_CHANNELS == 1 ? "mono" : "stereo");
    ESP_LOGI(TAG, "  Bit width: 16-bit");
#ifdef CONFIG_I2S1_BCK_GPIO
    ESP_LOGI(TAG, "  BCK=GPIO%d, WS=GPIO%d, DIN=GPIO%d",
             CONFIG_I2S1_BCK_GPIO, CONFIG_I2S1_WS_GPIO, CONFIG_I2S1_DIN_GPIO);
#else
    ESP_LOGI(TAG, "  BCK=GPIO15, WS=GPIO16, DIN=GPIO17 (defaults)");
#endif

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

    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "Initializing Omni-P4 Audio Pipeline");
#ifdef CONFIG_AUDIO_INPUT_USB
    ESP_LOGI(TAG, "  Input:  USB Audio (ReSpeaker/UAC 1.0)");
#else
    ESP_LOGI(TAG, "  Input:  I2S Microphone (I2S1)");
#endif
    ESP_LOGI(TAG, "  Output: ES9038Q2M DAC (I2S0)");
    ESP_LOGI(TAG, "============================================");

    // Create synchronization primitives
    s_audio.event_group = xEventGroupCreate();
    s_audio.mutex = xSemaphoreCreateMutex();
    if (!s_audio.event_group || !s_audio.mutex) {
        ESP_LOGE(TAG, "Failed to create sync primitives");
        return ESP_ERR_NO_MEM;
    }

    // ========================================
    // Allocate buffers in PSRAM
    // ========================================

    // Output buffer (to DAC)
    s_audio.output_buffer = heap_caps_malloc(AUDIO_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    if (!s_audio.output_buffer) {
        ESP_LOGE(TAG, "Failed to allocate output buffer");
        return ESP_ERR_NO_MEM;
    }

    // Raw input buffer: 48kHz stereo (high quality for local LLM)
    s_audio.input_buffer_raw = heap_caps_malloc(RAW_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    if (!s_audio.input_buffer_raw) {
        ESP_LOGE(TAG, "Failed to allocate raw input buffer");
        return ESP_ERR_NO_MEM;
    }

    // Processed input buffer: 16kHz mono (ESPHome/Whisper compatible)
    s_audio.input_buffer_processed = heap_caps_malloc(PROCESSED_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    if (!s_audio.input_buffer_processed) {
        ESP_LOGE(TAG, "Failed to allocate processed input buffer");
        return ESP_ERR_NO_MEM;
    }

    // Legacy alias for backward compatibility
    s_audio.input_buffer = s_audio.input_buffer_processed;

    ESP_LOGI(TAG, "Buffers allocated in PSRAM:");
    ESP_LOGI(TAG, "  Output (DAC):           %d KB", AUDIO_BUFFER_SIZE / 1024);
    ESP_LOGI(TAG, "  Raw (48kHz stereo):     %d KB", RAW_BUFFER_SIZE / 1024);
    ESP_LOGI(TAG, "  Processed (16kHz mono): %d KB", PROCESSED_BUFFER_SIZE / 1024);

    // ========================================
    // Initialize I2S0 Output (DAC)
    // ========================================
    esp_err_t ret = init_i2s0_output();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S0 (DAC) init failed");
        return ret;
    }

    // ========================================
    // Initialize Microphone Input (USB or I2S)
    // ========================================
#ifdef CONFIG_AUDIO_INPUT_USB
    // USB Audio Input (ReSpeaker, UAC 1.0 devices)
    ret = init_usb_audio_input();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB Audio Input init failed");
        return ret;
    }
    // USB audio will be ready when device connects (async)
    s_audio.mic_ready = false;
    s_audio.mic_streaming = false;
#else
    // I2S Microphone Input (INMP441, etc.)
    ret = init_i2s1_microphone();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S1 Microphone init failed");
        return ret;
    }
    s_audio.mic_ready = true;
    s_audio.mic_streaming = false;
#endif

    // Set defaults
    s_audio.volume = 70;
    s_audio.muted = false;
    s_audio.energy_threshold = -40.0f;  // dB threshold for VAD
    s_audio.state = AUDIO_STATE_IDLE;
    s_audio.downsample_counter = 0;

    // Reset buffer positions
    s_audio.raw_write_pos = 0;
    s_audio.raw_read_pos = 0;
    s_audio.processed_write_pos = 0;
    s_audio.processed_read_pos = 0;
    s_audio.input_write_pos = 0;
    s_audio.input_read_pos = 0;

    s_audio.initialized = true;

    // Enable I2S output channel (DAC)
    i2s_channel_enable(s_audio.i2s0_tx_handle);

#ifndef CONFIG_AUDIO_INPUT_USB
    // Enable I2S input channel and start mic task (I2S mode only)
    i2s_channel_enable(s_audio.i2s1_rx_handle);

    // Start microphone reading task
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        mic_read_task,
        "mic_task",
        4096,
        NULL,
        5,  // Priority
        &s_audio.mic_task_handle,
        1   // Core 1
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create mic task");
        return ESP_FAIL;
    }
#endif

    xEventGroupSetBits(s_audio.event_group, AUDIO_READY_BIT);

    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "Audio pipeline initialized successfully");
    ESP_LOGI(TAG, "  Dual buffer mode enabled:");
#ifdef CONFIG_AUDIO_INPUT_USB
    ESP_LOGI(TAG, "    - Raw 48kHz stereo -> Local LLM (Qwen2.5)");
    ESP_LOGI(TAG, "    - Processed 16kHz mono -> ESPHome/Whisper");
    ESP_LOGI(TAG, "  USB Audio: Waiting for device connection...");
#else
    ESP_LOGI(TAG, "    - Raw %d Hz %s  -> Local LLM (Qwen2.5)",
             CONFIG_MIC_SAMPLE_RATE, INPUT_CHANNELS == 2 ? "stereo" : "mono");
    ESP_LOGI(TAG, "    - Processed 16kHz mono -> ESPHome/Whisper");
#endif
    ESP_LOGI(TAG, "============================================");
    return ESP_OK;
}

void audio_pipeline_deinit(void)
{
    if (!s_audio.initialized) return;

    ESP_LOGI(TAG, "Deinitializing audio pipeline...");

#ifdef CONFIG_AUDIO_INPUT_USB
    // Deinit USB Audio Input
    usb_audio_input_deinit();
#else
    // Stop I2S microphone task
    s_audio.mic_ready = false;
    if (s_audio.mic_task_handle) {
        // Wait for task to exit
        vTaskDelay(pdMS_TO_TICKS(200));
        s_audio.mic_task_handle = NULL;
    }

    // Disable and delete I2S input channel (microphone)
    if (s_audio.i2s1_rx_handle) {
        i2s_channel_disable(s_audio.i2s1_rx_handle);
        i2s_del_channel(s_audio.i2s1_rx_handle);
        s_audio.i2s1_rx_handle = NULL;
    }
#endif

    // Disable and delete I2S output channel (DAC)
    if (s_audio.i2s0_tx_handle) {
        i2s_channel_disable(s_audio.i2s0_tx_handle);
        i2s_del_channel(s_audio.i2s0_tx_handle);
        s_audio.i2s0_tx_handle = NULL;
    }

    // Free buffers
    if (s_audio.output_buffer) {
        free(s_audio.output_buffer);
    }
    if (s_audio.input_buffer_raw) {
        free(s_audio.input_buffer_raw);
    }
    if (s_audio.input_buffer_processed) {
        free(s_audio.input_buffer_processed);
    }

    // Delete primitives
    if (s_audio.event_group) {
        vEventGroupDelete(s_audio.event_group);
    }
    if (s_audio.mutex) {
        vSemaphoreDelete(s_audio.mutex);
    }

    memset(&s_audio, 0, sizeof(s_audio));
    ESP_LOGI(TAG, "Audio pipeline deinitialized");
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

    // ========================================
    // Note: Audio input is handled by USB Audio Host callbacks
    // (usb_audio_data_callback) - no need to poll here
    // ========================================

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

/**
 * @brief Read processed audio (16kHz mono) - for ESPHome/cloud STT
 *
 * This is the legacy API that reads from the processed buffer.
 * Use this for ESPHome voice assistant integration.
 */
size_t audio_pipeline_read(uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (!s_audio.initialized || !data || len == 0) return 0;

    if (xSemaphoreTake(s_audio.mutex, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return 0;
    }

    size_t available = (s_audio.processed_write_pos - s_audio.processed_read_pos + PROCESSED_BUFFER_SIZE) % PROCESSED_BUFFER_SIZE;
    size_t to_read = (len < available) ? len : available;

    if (to_read > 0) {
        size_t first_chunk = PROCESSED_BUFFER_SIZE - s_audio.processed_read_pos;
        if (to_read <= first_chunk) {
            memcpy(data, s_audio.input_buffer_processed + s_audio.processed_read_pos, to_read);
        } else {
            memcpy(data, s_audio.input_buffer_processed + s_audio.processed_read_pos, first_chunk);
            memcpy(data + first_chunk, s_audio.input_buffer_processed, to_read - first_chunk);
        }
        s_audio.processed_read_pos = (s_audio.processed_read_pos + to_read) % PROCESSED_BUFFER_SIZE;

        // Sync legacy alias
        s_audio.input_read_pos = s_audio.processed_read_pos;
    }

    xSemaphoreGive(s_audio.mutex);
    return to_read;
}

/**
 * @brief Read raw audio (48kHz stereo) - for local LLM/high-quality processing
 *
 * Returns unprocessed audio data at full quality.
 * Use this for local voice recognition (Qwen2.5, Whisper, etc.)
 *
 * @param data Output buffer
 * @param len Maximum bytes to read
 * @param timeout_ms Timeout in milliseconds
 * @return Number of bytes actually read
 */
size_t audio_pipeline_read_raw(uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (!s_audio.initialized || !data || len == 0) return 0;

    if (xSemaphoreTake(s_audio.mutex, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return 0;
    }

    size_t available = (s_audio.raw_write_pos - s_audio.raw_read_pos + RAW_BUFFER_SIZE) % RAW_BUFFER_SIZE;
    size_t to_read = (len < available) ? len : available;

    if (to_read > 0) {
        size_t first_chunk = RAW_BUFFER_SIZE - s_audio.raw_read_pos;
        if (to_read <= first_chunk) {
            memcpy(data, s_audio.input_buffer_raw + s_audio.raw_read_pos, to_read);
        } else {
            memcpy(data, s_audio.input_buffer_raw + s_audio.raw_read_pos, first_chunk);
            memcpy(data + first_chunk, s_audio.input_buffer_raw, to_read - first_chunk);
        }
        s_audio.raw_read_pos = (s_audio.raw_read_pos + to_read) % RAW_BUFFER_SIZE;
    }

    xSemaphoreGive(s_audio.mutex);
    return to_read;
}

/**
 * @brief Get raw audio buffer info
 *
 * @param sample_rate Output: sample rate in Hz (48000)
 * @param channels Output: number of channels (1=mono or 2=stereo)
 * @param bit_depth Output: bits per sample (16)
 */
void audio_pipeline_get_raw_format(uint32_t *sample_rate, uint8_t *channels, uint8_t *bit_depth)
{
    if (sample_rate) *sample_rate = CONFIG_MIC_SAMPLE_RATE;
    if (channels) *channels = INPUT_CHANNELS;
    if (bit_depth) *bit_depth = 16;
}

/**
 * @brief Get processed audio buffer info
 *
 * @param sample_rate Output: sample rate in Hz (16000)
 * @param channels Output: number of channels (1 = mono)
 * @param bit_depth Output: bits per sample (16)
 */
void audio_pipeline_get_processed_format(uint32_t *sample_rate, uint8_t *channels, uint8_t *bit_depth)
{
    if (sample_rate) *sample_rate = CONFIG_PROCESSED_SAMPLE_RATE;  // 16000 Hz
    if (channels) *channels = 1;  // mono
    if (bit_depth) *bit_depth = 16;
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
        // Use processed buffer size (16kHz mono) for input level
        size_t used = (s_audio.processed_write_pos - s_audio.processed_read_pos + PROCESSED_BUFFER_SIZE) % PROCESSED_BUFFER_SIZE;
        *input_level = (used * 100) / PROCESSED_BUFFER_SIZE;
    }
}

void audio_pipeline_get_stats(uint32_t *underruns, uint32_t *overruns)
{
    if (underruns) *underruns = s_audio.underruns;
    if (overruns) *overruns = s_audio.overruns;
}
