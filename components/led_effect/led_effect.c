/**
 * @file led_effect.c
 * @brief Phantom LED Ring Effect Implementation
 */

#include "led_effect.h"
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static const char *TAG = "led_effect";

// ============================================================================
// WS2812B Timing (in RMT ticks at 10MHz)
// ============================================================================

#define WS2812_T0H_NS   350
#define WS2812_T0L_NS   900
#define WS2812_T1H_NS   900
#define WS2812_T1L_NS   350
#define WS2812_RESET_US 280

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000  // 10MHz

// ============================================================================
// Internal State
// ============================================================================

typedef struct {
    rmt_channel_handle_t rmt_channel;
    rmt_encoder_handle_t encoder;

    led_color_t *pixels;
    int led_count;
    uint8_t brightness;

    led_mode_t mode;
    led_mode_t prev_mode;
    led_color_t primary_color;
    led_color_t secondary_color;

    // Animation state
    uint32_t frame_count;
    float phase;
    float audio_level;

    // Flash state
    bool flash_active;
    led_color_t flash_color;
    int flash_count;

    bool initialized;
} led_state_t;

static led_state_t s_led = {0};

// ============================================================================
// RMT Encoder for WS2812B
// ============================================================================

typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
} rmt_led_strip_encoder_t;

static size_t rmt_encode_led_strip(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                                    const void *primary_data, size_t data_size,
                                    rmt_encode_state_t *ret_state)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder = led_encoder->bytes_encoder;
    rmt_encoder_handle_t copy_encoder = led_encoder->copy_encoder;
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;

    switch (led_encoder->state) {
    case 0:  // Encode RGB data
        encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, primary_data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_encoder->state = 1;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            *ret_state = (rmt_encode_state_t)(session_state & (~RMT_ENCODING_COMPLETE));
            return encoded_symbols;
        }
        // fall through
    case 1:  // Encode reset code
        encoded_symbols += copy_encoder->encode(copy_encoder, channel, &led_encoder->reset_code,
                                                 sizeof(led_encoder->reset_code), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_encoder->state = RMT_ENCODING_RESET;
            *ret_state = RMT_ENCODING_COMPLETE;
        }
        break;
    }

    return encoded_symbols;
}

static esp_err_t rmt_del_led_strip_encoder(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_del_encoder(led_encoder->bytes_encoder);
    rmt_del_encoder(led_encoder->copy_encoder);
    free(led_encoder);
    return ESP_OK;
}

static esp_err_t rmt_led_strip_encoder_reset(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_reset(led_encoder->bytes_encoder);
    rmt_encoder_reset(led_encoder->copy_encoder);
    led_encoder->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

static esp_err_t rmt_new_led_strip_encoder(rmt_encoder_handle_t *ret_encoder)
{
    rmt_led_strip_encoder_t *led_encoder = calloc(1, sizeof(rmt_led_strip_encoder_t));
    if (!led_encoder) {
        return ESP_ERR_NO_MEM;
    }

    led_encoder->base.encode = rmt_encode_led_strip;
    led_encoder->base.del = rmt_del_led_strip_encoder;
    led_encoder->base.reset = rmt_led_strip_encoder_reset;

    // Bytes encoder for RGB data
    rmt_bytes_encoder_config_t bytes_config = {
        .bit0 = {
            .level0 = 1,
            .duration0 = WS2812_T0H_NS * RMT_LED_STRIP_RESOLUTION_HZ / 1000000000,
            .level1 = 0,
            .duration1 = WS2812_T0L_NS * RMT_LED_STRIP_RESOLUTION_HZ / 1000000000,
        },
        .bit1 = {
            .level0 = 1,
            .duration0 = WS2812_T1H_NS * RMT_LED_STRIP_RESOLUTION_HZ / 1000000000,
            .level1 = 0,
            .duration1 = WS2812_T1L_NS * RMT_LED_STRIP_RESOLUTION_HZ / 1000000000,
        },
        .flags.msb_first = 1,
    };

    esp_err_t ret = rmt_new_bytes_encoder(&bytes_config, &led_encoder->bytes_encoder);
    if (ret != ESP_OK) {
        free(led_encoder);
        return ret;
    }

    // Copy encoder for reset code
    rmt_copy_encoder_config_t copy_config = {};
    ret = rmt_new_copy_encoder(&copy_config, &led_encoder->copy_encoder);
    if (ret != ESP_OK) {
        rmt_del_encoder(led_encoder->bytes_encoder);
        free(led_encoder);
        return ret;
    }

    // Reset code
    led_encoder->reset_code = (rmt_symbol_word_t){
        .level0 = 0,
        .duration0 = WS2812_RESET_US * RMT_LED_STRIP_RESOLUTION_HZ / 1000000,
        .level1 = 0,
        .duration1 = WS2812_RESET_US * RMT_LED_STRIP_RESOLUTION_HZ / 1000000,
    };

    *ret_encoder = &led_encoder->base;
    return ESP_OK;
}

// ============================================================================
// Helper Functions
// ============================================================================

static led_color_t scale_brightness(led_color_t color, uint8_t brightness)
{
    return (led_color_t){
        .r = (color.r * brightness) / 255,
        .g = (color.g * brightness) / 255,
        .b = (color.b * brightness) / 255,
    };
}

static led_color_t blend_colors(led_color_t c1, led_color_t c2, float t)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return (led_color_t){
        .r = (uint8_t)(c1.r * (1 - t) + c2.r * t),
        .g = (uint8_t)(c1.g * (1 - t) + c2.g * t),
        .b = (uint8_t)(c1.b * (1 - t) + c2.b * t),
    };
}

static void write_pixels_to_strip(void)
{
    if (!s_led.initialized || !s_led.rmt_channel) return;

    // Convert to GRB format for WS2812B
    uint8_t *grb_data = malloc(s_led.led_count * 3);
    if (!grb_data) return;

    for (int i = 0; i < s_led.led_count; i++) {
        led_color_t c = scale_brightness(s_led.pixels[i], s_led.brightness);
        grb_data[i * 3 + 0] = c.g;
        grb_data[i * 3 + 1] = c.r;
        grb_data[i * 3 + 2] = c.b;
    }

    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };
    rmt_transmit(s_led.rmt_channel, s_led.encoder, grb_data, s_led.led_count * 3, &tx_config);
    rmt_tx_wait_all_done(s_led.rmt_channel, portMAX_DELAY);

    free(grb_data);
}

// ============================================================================
// Animation Effects
// ============================================================================

static void effect_breathing(void)
{
    // Soft sine wave breathing
    float breath = (sinf(s_led.phase) + 1.0f) / 2.0f;
    breath = 0.1f + breath * 0.9f;  // Min 10% brightness

    led_color_t c = scale_brightness(s_led.primary_color, (uint8_t)(breath * 255));

    for (int i = 0; i < s_led.led_count; i++) {
        s_led.pixels[i] = c;
    }

    s_led.phase += 0.05f;
    if (s_led.phase > 2 * M_PI) s_led.phase -= 2 * M_PI;
}

static void effect_listening(void)
{
    // Wave effect spreading from center
    int center = s_led.led_count / 2;

    for (int i = 0; i < s_led.led_count; i++) {
        float dist = fabsf((float)(i - center)) / center;
        float wave = sinf(s_led.phase - dist * M_PI);
        wave = (wave + 1.0f) / 2.0f;

        s_led.pixels[i] = blend_colors(LED_COLOR_OFF, s_led.secondary_color, wave);
    }

    s_led.phase += 0.15f;
    if (s_led.phase > 2 * M_PI) s_led.phase -= 2 * M_PI;
}

static void effect_thinking(void)
{
    // Rotating dots
    float dot_pos = fmodf(s_led.phase, 2 * M_PI) / (2 * M_PI) * s_led.led_count;

    for (int i = 0; i < s_led.led_count; i++) {
        float dist = fabsf((float)i - dot_pos);
        if (dist > s_led.led_count / 2) dist = s_led.led_count - dist;

        float brightness = 1.0f - (dist / 3.0f);
        if (brightness < 0) brightness = 0;

        s_led.pixels[i] = blend_colors(LED_COLOR_OFF, s_led.primary_color, brightness);
    }

    s_led.phase += 0.2f;
}

static void effect_speaking(void)
{
    // Audio visualizer
    float level = s_led.audio_level;
    int lit_count = (int)(level * s_led.led_count);

    for (int i = 0; i < s_led.led_count; i++) {
        // Symmetric from center
        int dist_from_center = abs(i - s_led.led_count / 2);
        bool lit = (dist_from_center * 2 < lit_count);

        if (lit) {
            float t = (float)dist_from_center / (s_led.led_count / 2);
            s_led.pixels[i] = blend_colors(s_led.primary_color, s_led.secondary_color, t);
        } else {
            s_led.pixels[i] = LED_COLOR_OFF;
        }
    }
}

static void effect_notification(void)
{
    // Pulsing attention getter
    float pulse = (sinf(s_led.phase * 3) + 1.0f) / 2.0f;

    for (int i = 0; i < s_led.led_count; i++) {
        s_led.pixels[i] = blend_colors(LED_COLOR_OFF, LED_COLOR_ORANGE, pulse);
    }

    s_led.phase += 0.1f;
}

static void effect_wifi_connecting(void)
{
    // Spinning blue loader
    int pos = (int)(s_led.phase / 0.5f) % s_led.led_count;

    for (int i = 0; i < s_led.led_count; i++) {
        int dist = abs(i - pos);
        if (dist > s_led.led_count / 2) dist = s_led.led_count - dist;

        float brightness = 1.0f - (float)dist / 4.0f;
        if (brightness < 0.1f) brightness = 0.1f;

        s_led.pixels[i] = blend_colors(LED_COLOR_OFF, LED_COLOR_BLUE, brightness);
    }

    s_led.phase += 0.15f;
}

// ============================================================================
// Public API Implementation
// ============================================================================

esp_err_t led_effect_init(void)
{
    led_effect_config_t config = LED_EFFECT_DEFAULT_CONFIG();
    return led_effect_init_config(&config);
}

esp_err_t led_effect_init_config(const led_effect_config_t *config)
{
    if (s_led.initialized) {
        ESP_LOGW(TAG, "LED effect already initialized");
        return ESP_OK;
    }

    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing LED strip: GPIO%d, %d LEDs", config->gpio_num, config->led_count);

    // Allocate pixel buffer
    s_led.pixels = calloc(config->led_count, sizeof(led_color_t));
    if (!s_led.pixels) {
        return ESP_ERR_NO_MEM;
    }

    s_led.led_count = config->led_count;
    s_led.brightness = config->brightness;
    s_led.primary_color = config->primary_color;
    s_led.secondary_color = config->secondary_color;

    // Configure RMT TX channel
    rmt_tx_channel_config_t tx_config = {
        .gpio_num = config->gpio_num,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };

    esp_err_t ret = rmt_new_tx_channel(&tx_config, &s_led.rmt_channel);
    if (ret != ESP_OK) {
        free(s_led.pixels);
        return ret;
    }

    // Create encoder
    ret = rmt_new_led_strip_encoder(&s_led.encoder);
    if (ret != ESP_OK) {
        rmt_del_channel(s_led.rmt_channel);
        free(s_led.pixels);
        return ret;
    }

    // Enable channel
    ret = rmt_enable(s_led.rmt_channel);
    if (ret != ESP_OK) {
        rmt_del_encoder(s_led.encoder);
        rmt_del_channel(s_led.rmt_channel);
        free(s_led.pixels);
        return ret;
    }

    s_led.mode = LED_MODE_OFF;
    s_led.initialized = true;

    // Clear strip
    led_effect_clear();
    led_effect_refresh();

    ESP_LOGI(TAG, "LED strip initialized successfully");
    return ESP_OK;
}

void led_effect_deinit(void)
{
    if (!s_led.initialized) return;

    led_effect_clear();
    led_effect_refresh();

    if (s_led.rmt_channel) {
        rmt_disable(s_led.rmt_channel);
        rmt_del_channel(s_led.rmt_channel);
    }

    if (s_led.encoder) {
        rmt_del_encoder(s_led.encoder);
    }

    if (s_led.pixels) {
        free(s_led.pixels);
    }

    memset(&s_led, 0, sizeof(s_led));
}

void led_effect_update(void)
{
    if (!s_led.initialized) return;

    // Handle flash override
    if (s_led.flash_active) {
        if (s_led.flash_count > 0) {
            bool on = (s_led.frame_count % 10) < 5;
            for (int i = 0; i < s_led.led_count; i++) {
                s_led.pixels[i] = on ? s_led.flash_color : LED_COLOR_OFF;
            }
            if (s_led.frame_count % 10 == 9) {
                s_led.flash_count--;
            }
        } else {
            s_led.flash_active = false;
        }
        write_pixels_to_strip();
        s_led.frame_count++;
        return;
    }

    // Run effect based on mode
    switch (s_led.mode) {
    case LED_MODE_OFF:
        led_effect_clear();
        break;
    case LED_MODE_BREATHING:
        effect_breathing();
        break;
    case LED_MODE_LISTENING:
        effect_listening();
        break;
    case LED_MODE_THINKING:
        effect_thinking();
        break;
    case LED_MODE_SPEAKING:
        effect_speaking();
        break;
    case LED_MODE_NOTIFICATION:
        effect_notification();
        break;
    case LED_MODE_WIFI_CONNECTING:
        effect_wifi_connecting();
        break;
    case LED_MODE_SUCCESS:
        for (int i = 0; i < s_led.led_count; i++) {
            s_led.pixels[i] = LED_COLOR_GREEN;
        }
        break;
    case LED_MODE_ERROR:
        for (int i = 0; i < s_led.led_count; i++) {
            s_led.pixels[i] = LED_COLOR_RED;
        }
        break;
    default:
        break;
    }

    write_pixels_to_strip();
    s_led.frame_count++;
}

void led_effect_set_mode(led_mode_t mode)
{
    if (mode != s_led.mode) {
        s_led.prev_mode = s_led.mode;
        s_led.mode = mode;
        s_led.phase = 0;
        ESP_LOGD(TAG, "LED mode changed: %d -> %d", s_led.prev_mode, mode);
    }
}

led_mode_t led_effect_get_mode(void)
{
    return s_led.mode;
}

void led_effect_set_brightness(uint8_t brightness)
{
    s_led.brightness = brightness;
}

void led_effect_set_primary_color(led_color_t color)
{
    s_led.primary_color = color;
}

void led_effect_set_secondary_color(led_color_t color)
{
    s_led.secondary_color = color;
}

void led_effect_flash_error(void)
{
    s_led.flash_active = true;
    s_led.flash_color = LED_COLOR_RED;
    s_led.flash_count = 3;
}

void led_effect_flash_success(void)
{
    s_led.flash_active = true;
    s_led.flash_color = LED_COLOR_GREEN;
    s_led.flash_count = 2;
}

void led_effect_set_pixel(int index, led_color_t color)
{
    if (index >= 0 && index < s_led.led_count) {
        s_led.pixels[index] = color;
    }
}

void led_effect_set_all(led_color_t color)
{
    for (int i = 0; i < s_led.led_count; i++) {
        s_led.pixels[i] = color;
    }
}

void led_effect_clear(void)
{
    led_effect_set_all(LED_COLOR_OFF);
}

void led_effect_refresh(void)
{
    write_pixels_to_strip();
}

void led_effect_set_audio_level(float level)
{
    if (level < 0) level = 0;
    if (level > 1) level = 1;
    s_led.audio_level = level;
}
