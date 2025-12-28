/**
 * @file display_manager.c
 * @brief Display Manager Implementation
 */

#include "display_manager.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "lvgl.h"
#include "sdkconfig.h"

static const char *TAG = "display_mgr";

// ============================================================================
// LVGL Configuration
// ============================================================================

#define LVGL_TICK_PERIOD_MS     2
#define LVGL_BUFFER_LINES       50

// ============================================================================
// Internal State
// ============================================================================

typedef struct {
    // Display handles
    esp_lcd_panel_handle_t panel;
    esp_lcd_dsi_bus_handle_t dsi_bus;

    // LVGL
    lv_display_t *display;
    SemaphoreHandle_t lvgl_mutex;

    // Screens
    lv_obj_t *screens[SCREEN_COUNT];
    screen_type_t current_screen;

    // UI Elements (Home Screen)
    lv_obj_t *lbl_time;
    lv_obj_t *lbl_date;
    lv_obj_t *lbl_temp;
    lv_obj_t *lbl_humidity;

    // UI Elements (Sensor Screen)
    lv_obj_t *lbl_sensor_temp;
    lv_obj_t *lbl_sensor_hum;
    lv_obj_t *lbl_sensor_co2;
    lv_obj_t *lbl_sensor_tvoc;
    lv_obj_t *lbl_sensor_pressure;
    lv_obj_t *lbl_sensor_lux;
    lv_obj_t *lbl_sensor_aqi;
    lv_obj_t *bar_co2;
    lv_obj_t *bar_aqi;

    // UI Elements (Music Screen)
    lv_obj_t *lbl_track_title;
    lv_obj_t *lbl_track_artist;
    lv_obj_t *bar_progress;
    lv_obj_t *btn_play_pause;
    lv_obj_t *spectrum_bars[16];

    // Notification
    lv_obj_t *notification_popup;
    lv_timer_t *notification_timer;

    // State
    uint8_t brightness;
    bool power_on;
    bool initialized;

} display_state_t;

static display_state_t s_disp = {0};

// ============================================================================
// LVGL Tick Timer Callback
// ============================================================================

static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

// ============================================================================
// LVGL Flush Callback
// ============================================================================

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel = lv_display_get_user_data(disp);

    int x_start = area->x1;
    int y_start = area->y1;
    int x_end = area->x2 + 1;
    int y_end = area->y2 + 1;

    esp_lcd_panel_draw_bitmap(panel, x_start, y_start, x_end, y_end, px_map);

    lv_display_flush_ready(disp);
}

// ============================================================================
// UI Creation
// ============================================================================

static void create_home_screen(void)
{
    s_disp.screens[SCREEN_HOME] = lv_obj_create(NULL);
    lv_obj_t *scr = s_disp.screens[SCREEN_HOME];

    // Set dark theme background
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);

    // Time display (large, centered)
    s_disp.lbl_time = lv_label_create(scr);
    lv_obj_set_style_text_font(s_disp.lbl_time, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_disp.lbl_time, lv_color_hex(0xffffff), 0);
    lv_label_set_text(s_disp.lbl_time, "12:00");
    lv_obj_align(s_disp.lbl_time, LV_ALIGN_CENTER, 0, -50);

    // Date display
    s_disp.lbl_date = lv_label_create(scr);
    lv_obj_set_style_text_font(s_disp.lbl_date, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_disp.lbl_date, lv_color_hex(0xaaaaaa), 0);
    lv_label_set_text(s_disp.lbl_date, "Monday, January 1");
    lv_obj_align(s_disp.lbl_date, LV_ALIGN_CENTER, 0, 10);

    // Temperature display
    s_disp.lbl_temp = lv_label_create(scr);
    lv_obj_set_style_text_font(s_disp.lbl_temp, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_disp.lbl_temp, lv_color_hex(0x4ecdc4), 0);
    lv_label_set_text(s_disp.lbl_temp, "25°C");
    lv_obj_align(s_disp.lbl_temp, LV_ALIGN_BOTTOM_LEFT, 30, -30);

    // Humidity display
    s_disp.lbl_humidity = lv_label_create(scr);
    lv_obj_set_style_text_font(s_disp.lbl_humidity, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_disp.lbl_humidity, lv_color_hex(0x45b7d1), 0);
    lv_label_set_text(s_disp.lbl_humidity, "50%");
    lv_obj_align(s_disp.lbl_humidity, LV_ALIGN_BOTTOM_RIGHT, -30, -30);
}

static void create_sensor_screen(void)
{
    s_disp.screens[SCREEN_SENSORS] = lv_obj_create(NULL);
    lv_obj_t *scr = s_disp.screens[SCREEN_SENSORS];

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0f0f23), 0);

    // Title
    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_label_set_text(title, "Environment Sensors");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    // Grid layout for sensor values
    int y_offset = 80;
    int y_spacing = 60;

    // Temperature
    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Temperature:");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xaaaaaa), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 30, y_offset);

    s_disp.lbl_sensor_temp = lv_label_create(scr);
    lv_obj_set_style_text_color(s_disp.lbl_sensor_temp, lv_color_hex(0x4ecdc4), 0);
    lv_label_set_text(s_disp.lbl_sensor_temp, "--°C");
    lv_obj_align(s_disp.lbl_sensor_temp, LV_ALIGN_TOP_RIGHT, -30, y_offset);

    // Humidity
    y_offset += y_spacing;
    lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Humidity:");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xaaaaaa), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 30, y_offset);

    s_disp.lbl_sensor_hum = lv_label_create(scr);
    lv_obj_set_style_text_color(s_disp.lbl_sensor_hum, lv_color_hex(0x45b7d1), 0);
    lv_label_set_text(s_disp.lbl_sensor_hum, "--%");
    lv_obj_align(s_disp.lbl_sensor_hum, LV_ALIGN_TOP_RIGHT, -30, y_offset);

    // CO2
    y_offset += y_spacing;
    lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "CO2:");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xaaaaaa), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 30, y_offset);

    s_disp.lbl_sensor_co2 = lv_label_create(scr);
    lv_obj_set_style_text_color(s_disp.lbl_sensor_co2, lv_color_hex(0xf9ca24), 0);
    lv_label_set_text(s_disp.lbl_sensor_co2, "-- ppm");
    lv_obj_align(s_disp.lbl_sensor_co2, LV_ALIGN_TOP_RIGHT, -30, y_offset);

    // TVOC
    y_offset += y_spacing;
    lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "TVOC:");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xaaaaaa), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 30, y_offset);

    s_disp.lbl_sensor_tvoc = lv_label_create(scr);
    lv_obj_set_style_text_color(s_disp.lbl_sensor_tvoc, lv_color_hex(0xf39c12), 0);
    lv_label_set_text(s_disp.lbl_sensor_tvoc, "-- ppb");
    lv_obj_align(s_disp.lbl_sensor_tvoc, LV_ALIGN_TOP_RIGHT, -30, y_offset);

    // Pressure
    y_offset += y_spacing;
    lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Pressure:");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xaaaaaa), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 30, y_offset);

    s_disp.lbl_sensor_pressure = lv_label_create(scr);
    lv_obj_set_style_text_color(s_disp.lbl_sensor_pressure, lv_color_hex(0x9b59b6), 0);
    lv_label_set_text(s_disp.lbl_sensor_pressure, "-- hPa");
    lv_obj_align(s_disp.lbl_sensor_pressure, LV_ALIGN_TOP_RIGHT, -30, y_offset);

    // Light
    y_offset += y_spacing;
    lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Light:");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xaaaaaa), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 30, y_offset);

    s_disp.lbl_sensor_lux = lv_label_create(scr);
    lv_obj_set_style_text_color(s_disp.lbl_sensor_lux, lv_color_hex(0xf1c40f), 0);
    lv_label_set_text(s_disp.lbl_sensor_lux, "-- lux");
    lv_obj_align(s_disp.lbl_sensor_lux, LV_ALIGN_TOP_RIGHT, -30, y_offset);

    // AQI
    y_offset += y_spacing;
    lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Air Quality:");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xaaaaaa), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 30, y_offset);

    s_disp.lbl_sensor_aqi = lv_label_create(scr);
    lv_obj_set_style_text_color(s_disp.lbl_sensor_aqi, lv_color_hex(0x2ecc71), 0);
    lv_label_set_text(s_disp.lbl_sensor_aqi, "Good");
    lv_obj_align(s_disp.lbl_sensor_aqi, LV_ALIGN_TOP_RIGHT, -30, y_offset);
}

static void create_music_screen(void)
{
    s_disp.screens[SCREEN_MUSIC] = lv_obj_create(NULL);
    lv_obj_t *scr = s_disp.screens[SCREEN_MUSIC];

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x16213e), 0);

    // Track title
    s_disp.lbl_track_title = lv_label_create(scr);
    lv_obj_set_style_text_font(s_disp.lbl_track_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_disp.lbl_track_title, lv_color_hex(0xffffff), 0);
    lv_label_set_text(s_disp.lbl_track_title, "No Track");
    lv_obj_align(s_disp.lbl_track_title, LV_ALIGN_TOP_MID, 0, 100);

    // Artist
    s_disp.lbl_track_artist = lv_label_create(scr);
    lv_obj_set_style_text_color(s_disp.lbl_track_artist, lv_color_hex(0xaaaaaa), 0);
    lv_label_set_text(s_disp.lbl_track_artist, "Unknown Artist");
    lv_obj_align(s_disp.lbl_track_artist, LV_ALIGN_TOP_MID, 0, 140);

    // Progress bar
    s_disp.bar_progress = lv_bar_create(scr);
    lv_obj_set_size(s_disp.bar_progress, 400, 8);
    lv_bar_set_range(s_disp.bar_progress, 0, 100);
    lv_bar_set_value(s_disp.bar_progress, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_disp.bar_progress, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_disp.bar_progress, lv_color_hex(0x4ecdc4), LV_PART_INDICATOR);
    lv_obj_align(s_disp.bar_progress, LV_ALIGN_CENTER, 0, 50);

    // Spectrum visualizer (16 bars)
    int bar_width = 20;
    int bar_spacing = 5;
    int total_width = 16 * bar_width + 15 * bar_spacing;
    int start_x = -total_width / 2;

    for (int i = 0; i < 16; i++) {
        s_disp.spectrum_bars[i] = lv_bar_create(scr);
        lv_obj_set_size(s_disp.spectrum_bars[i], bar_width, 150);
        lv_bar_set_range(s_disp.spectrum_bars[i], 0, 100);
        lv_bar_set_value(s_disp.spectrum_bars[i], 10, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(s_disp.spectrum_bars[i], lv_color_hex(0x222222), LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_disp.spectrum_bars[i], lv_color_hex(0x4ecdc4), LV_PART_INDICATOR);
        lv_obj_align(s_disp.spectrum_bars[i], LV_ALIGN_BOTTOM_MID,
                     start_x + i * (bar_width + bar_spacing) + bar_width / 2, -50);
    }
}

static void create_notification_popup(void)
{
    s_disp.notification_popup = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_disp.notification_popup, 400, 100);
    lv_obj_align(s_disp.notification_popup, LV_ALIGN_TOP_MID, 0, -110);
    lv_obj_set_style_bg_color(s_disp.notification_popup, lv_color_hex(0x2d3436), 0);
    lv_obj_set_style_radius(s_disp.notification_popup, 15, 0);
    lv_obj_add_flag(s_disp.notification_popup, LV_OBJ_FLAG_HIDDEN);
}

// ============================================================================
// Backlight Control
// ============================================================================

static void init_backlight(void)
{
#if CONFIG_DISPLAY_BACKLIGHT_GPIO >= 0
    // Configure LEDC for PWM backlight control
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 25000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t channel_conf = {
        .gpio_num = CONFIG_DISPLAY_BACKLIGHT_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&channel_conf);

    // Set initial brightness
    display_manager_set_brightness(80);
#endif
}

// ============================================================================
// Public API Implementation
// ============================================================================

esp_err_t display_manager_init(void)
{
    if (s_disp.initialized) {
        ESP_LOGW(TAG, "Display already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing display (%dx%d)...", DISPLAY_WIDTH, DISPLAY_HEIGHT);

    // Create LVGL mutex
    s_disp.lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    if (!s_disp.lvgl_mutex) {
        return ESP_ERR_NO_MEM;
    }

    // Initialize LVGL
    lv_init();

    // Create LVGL display
    s_disp.display = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if (!s_disp.display) {
        ESP_LOGE(TAG, "Failed to create LVGL display");
        return ESP_FAIL;
    }

    // Allocate draw buffers in PSRAM
    size_t buf_size = DISPLAY_WIDTH * LVGL_BUFFER_LINES * sizeof(lv_color_t);
    void *buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    void *buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);

    if (!buf1 || !buf2) {
        ESP_LOGE(TAG, "Failed to allocate LVGL buffers");
        return ESP_ERR_NO_MEM;
    }

    lv_display_set_buffers(s_disp.display, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp.display, lvgl_flush_cb);

    // TODO: Initialize MIPI-DSI panel
    // This is board-specific and requires ESP-BSP or custom driver
    // For now, we'll use a placeholder

    // Set user data for flush callback
    // lv_display_set_user_data(s_disp.display, s_disp.panel);

    // Initialize tick timer
    const esp_timer_create_args_t timer_args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer;
    esp_timer_create(&timer_args, &tick_timer);
    esp_timer_start_periodic(tick_timer, LVGL_TICK_PERIOD_MS * 1000);

    // Initialize backlight
    init_backlight();

    // Create UI screens
    create_home_screen();
    create_sensor_screen();
    create_music_screen();
    create_notification_popup();

    // Set initial screen
    s_disp.current_screen = SCREEN_HOME;
    lv_screen_load(s_disp.screens[SCREEN_HOME]);

    s_disp.brightness = 80;
    s_disp.power_on = true;
    s_disp.initialized = true;

    ESP_LOGI(TAG, "Display initialized successfully");
    return ESP_OK;
}

void display_manager_deinit(void)
{
    if (!s_disp.initialized) return;

    // TODO: Cleanup MIPI-DSI panel

    if (s_disp.lvgl_mutex) {
        vSemaphoreDelete(s_disp.lvgl_mutex);
    }

    memset(&s_disp, 0, sizeof(s_disp));
}

bool display_manager_lock(int timeout_ms)
{
    if (!s_disp.lvgl_mutex) return false;

    TickType_t timeout = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(s_disp.lvgl_mutex, timeout) == pdTRUE;
}

void display_manager_unlock(void)
{
    if (s_disp.lvgl_mutex) {
        xSemaphoreGiveRecursive(s_disp.lvgl_mutex);
    }
}

uint32_t display_manager_timer_handler(void)
{
    return lv_timer_handler();
}

void display_manager_set_screen(screen_type_t screen)
{
    if (screen >= SCREEN_COUNT || !s_disp.screens[screen]) return;

    if (display_manager_lock(100)) {
        s_disp.current_screen = screen;
        lv_screen_load_anim(s_disp.screens[screen], LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);
        display_manager_unlock();
    }
}

screen_type_t display_manager_get_screen(void)
{
    return s_disp.current_screen;
}

void display_manager_update_sensors(const sensor_data_t *data)
{
    if (!data || !s_disp.initialized) return;

    if (display_manager_lock(50)) {
        char buf[32];

        // Update home screen
        if (data->sht40_valid) {
            snprintf(buf, sizeof(buf), "%.1f°C", data->temperature);
            lv_label_set_text(s_disp.lbl_temp, buf);
            snprintf(buf, sizeof(buf), "%.0f%%", data->humidity);
            lv_label_set_text(s_disp.lbl_humidity, buf);
        }

        // Update sensor screen
        if (data->sht40_valid) {
            snprintf(buf, sizeof(buf), "%.1f°C", data->temperature);
            lv_label_set_text(s_disp.lbl_sensor_temp, buf);
            snprintf(buf, sizeof(buf), "%.1f%%", data->humidity);
            lv_label_set_text(s_disp.lbl_sensor_hum, buf);
        }

        if (data->scd41_valid) {
            snprintf(buf, sizeof(buf), "%d ppm", data->co2);
            lv_label_set_text(s_disp.lbl_sensor_co2, buf);

            // Color based on CO2 level
            lv_color_t color;
            if (data->co2 < 800) {
                color = lv_color_hex(0x2ecc71);  // Green
            } else if (data->co2 < 1000) {
                color = lv_color_hex(0xf1c40f);  // Yellow
            } else if (data->co2 < 1500) {
                color = lv_color_hex(0xe67e22);  // Orange
            } else {
                color = lv_color_hex(0xe74c3c);  // Red
            }
            lv_obj_set_style_text_color(s_disp.lbl_sensor_co2, color, 0);
        }

        if (data->ens160_valid) {
            snprintf(buf, sizeof(buf), "%d ppb", data->tvoc);
            lv_label_set_text(s_disp.lbl_sensor_tvoc, buf);
            lv_label_set_text(s_disp.lbl_sensor_aqi, sensor_hub_aqi_description(data->aqi));
        }

        if (data->bmp388_valid) {
            snprintf(buf, sizeof(buf), "%.1f hPa", data->pressure);
            lv_label_set_text(s_disp.lbl_sensor_pressure, buf);
        }

        if (data->bh1750_valid) {
            snprintf(buf, sizeof(buf), "%.0f lux", data->lux);
            lv_label_set_text(s_disp.lbl_sensor_lux, buf);
        }

        display_manager_unlock();
    }
}

void display_manager_update_time(int hour, int minute)
{
    if (!s_disp.initialized) return;

    if (display_manager_lock(50)) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);
        lv_label_set_text(s_disp.lbl_time, buf);
        display_manager_unlock();
    }
}

void display_manager_update_music(const char *title, const char *artist,
                                   float progress, bool is_playing)
{
    if (!s_disp.initialized) return;

    if (display_manager_lock(50)) {
        if (title) {
            lv_label_set_text(s_disp.lbl_track_title, title);
        }
        if (artist) {
            lv_label_set_text(s_disp.lbl_track_artist, artist);
        }
        lv_bar_set_value(s_disp.bar_progress, (int)(progress * 100), LV_ANIM_ON);
        display_manager_unlock();
    }
}

void display_manager_update_spectrum(const float *spectrum, int num_bands)
{
    if (!s_disp.initialized || !spectrum) return;

    if (display_manager_lock(20)) {
        for (int i = 0; i < 16 && i < num_bands; i++) {
            int value = (int)(spectrum[i] * 100);
            if (value > 100) value = 100;
            if (value < 5) value = 5;
            lv_bar_set_value(s_disp.spectrum_bars[i], value, LV_ANIM_ON);
        }
        display_manager_unlock();
    }
}

void display_manager_show_notification(const char *title, const char *message,
                                        uint32_t duration_ms)
{
    if (!s_disp.initialized || !s_disp.notification_popup) return;

    if (display_manager_lock(100)) {
        // TODO: Update notification content and animate in
        lv_obj_clear_flag(s_disp.notification_popup, LV_OBJ_FLAG_HIDDEN);
        display_manager_unlock();
    }
}

void display_manager_set_brightness(uint8_t brightness)
{
    if (brightness > 100) brightness = 100;
    s_disp.brightness = brightness;

#if CONFIG_DISPLAY_BACKLIGHT_GPIO >= 0
    uint32_t duty = (brightness * 1023) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
#endif
}

uint8_t display_manager_get_brightness(void)
{
    return s_disp.brightness;
}

void display_manager_set_power(bool on)
{
    s_disp.power_on = on;
    display_manager_set_brightness(on ? s_disp.brightness : 0);
}

bool display_manager_is_initialized(void)
{
    return s_disp.initialized;
}
