/**
 * @file ui_app.c
 * @brief Advanced UI Application Implementation
 *
 * Glass Cockpit Style Dashboard with TileView Navigation
 */

#include "ui_app.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "remo_client.h"

static const char *TAG = "ui_app";

// ============================================================================
// Color Palette (Aviation/Cockpit Theme)
// ============================================================================

#define COLOR_BG_DARK       lv_color_hex(0x0a0a0f)
#define COLOR_BG_PANEL      lv_color_hex(0x141428)
#define COLOR_ACCENT        lv_color_hex(0x00ff88)
#define COLOR_ACCENT2       lv_color_hex(0x00aaff)
#define COLOR_WARNING       lv_color_hex(0xffaa00)
#define COLOR_DANGER        lv_color_hex(0xff3333)
#define COLOR_TEXT          lv_color_hex(0xeeffee)
#define COLOR_TEXT_DIM      lv_color_hex(0x88aa88)
#define COLOR_GRID          lv_color_hex(0x224422)

// ============================================================================
// UI State
// ============================================================================

typedef struct {
    // Main containers
    lv_obj_t *tileview;
    lv_obj_t *tiles[UI_TILE_COUNT];

    // Standby overlay
    lv_obj_t *standby_layer;
    lv_obj_t *standby_clock;
    bool standby_active;

    // Glass Cockpit (Home tile)
    lv_obj_t *chart_waveform;
    lv_chart_series_t *wave_series;
    lv_obj_t *meter_co2;
    lv_meter_indicator_t *co2_needle;
    lv_obj_t *bar_temp;
    lv_obj_t *lbl_temp_value;
    lv_obj_t *lbl_humidity;
    lv_obj_t *lbl_pressure;
    lv_obj_t *lbl_time_small;

    // Control Center (Control tile)
    lv_obj_t *tabview;
    lv_obj_t *tab_lights;
    lv_obj_t *tab_climate;
    lv_obj_t *tab_media;

    bool initialized;
} ui_app_state_t;

static ui_app_state_t s_ui = {0};

// ============================================================================
// Event Handlers
// ============================================================================

static void standby_swipe_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == LV_DIR_TOP) {
        // Swipe up to dismiss standby
        ui_app_set_standby(false);
    }
}

static void remo_btn_cb(lv_event_t *e)
{
    const char *app_name = (const char *)lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_target(e);

    // Get signal name from button text
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    const char *signal = lv_label_get_text(label);

    ESP_LOGI(TAG, "Remo: %s -> %s", app_name, signal);

    // Send command via remo_client
    esp_err_t ret = remo_client_send_quick(app_name, signal);
    if (ret == ESP_OK) {
        // Flash button green briefly
        lv_obj_set_style_bg_color(btn, COLOR_ACCENT, 0);
        // Reset after delay (would need timer)
    } else {
        // Flash button red
        lv_obj_set_style_bg_color(btn, COLOR_DANGER, 0);
    }
}

// ============================================================================
// Standby Overlay Creation
// ============================================================================

static void create_standby_overlay(void)
{
    s_ui.standby_layer = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_ui.standby_layer, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_ui.standby_layer, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_ui.standby_layer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ui.standby_layer, 0, 0);
    lv_obj_set_style_radius(s_ui.standby_layer, 0, 0);
    lv_obj_set_style_pad_all(s_ui.standby_layer, 0, 0);
    lv_obj_align(s_ui.standby_layer, LV_ALIGN_CENTER, 0, 0);

    // Large digital clock
    s_ui.standby_clock = lv_label_create(s_ui.standby_layer);
    lv_obj_set_style_text_font(s_ui.standby_clock, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_ui.standby_clock, COLOR_TEXT, 0);
    lv_label_set_text(s_ui.standby_clock, "00:00:00");
    lv_obj_align(s_ui.standby_clock, LV_ALIGN_CENTER, 0, 0);

    // Swipe hint
    lv_obj_t *hint = lv_label_create(s_ui.standby_layer);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_DIM, 0);
    lv_label_set_text(hint, "Swipe up to unlock");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -50);

    // Add gesture detection
    lv_obj_add_event_cb(s_ui.standby_layer, standby_swipe_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_flag(s_ui.standby_layer, LV_OBJ_FLAG_GESTURE_BUBBLE);

    // Start hidden
    lv_obj_add_flag(s_ui.standby_layer, LV_OBJ_FLAG_HIDDEN);
    s_ui.standby_active = false;
}

// ============================================================================
// Glass Cockpit (Home Tile)
// ============================================================================

static void create_glass_cockpit(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, COLOR_BG_DARK, 0);
    lv_obj_set_style_pad_all(parent, 10, 0);

    // Title
    lv_obj_t *title = lv_label_create(parent);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_label_set_text(title, "OMNI-P4 COCKPIT");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);

    // Small time display
    s_ui.lbl_time_small = lv_label_create(parent);
    lv_obj_set_style_text_color(s_ui.lbl_time_small, COLOR_TEXT_DIM, 0);
    lv_label_set_text(s_ui.lbl_time_small, "12:00");
    lv_obj_align(s_ui.lbl_time_small, LV_ALIGN_TOP_RIGHT, -10, 5);

    // =============================================
    // Left Panel: Waveform Oscilloscope
    // =============================================
    lv_obj_t *wave_panel = lv_obj_create(parent);
    lv_obj_set_size(wave_panel, 350, 200);
    lv_obj_set_style_bg_color(wave_panel, COLOR_BG_PANEL, 0);
    lv_obj_set_style_border_color(wave_panel, COLOR_GRID, 0);
    lv_obj_set_style_border_width(wave_panel, 1, 0);
    lv_obj_set_style_radius(wave_panel, 5, 0);
    lv_obj_set_style_pad_all(wave_panel, 5, 0);
    lv_obj_align(wave_panel, LV_ALIGN_LEFT_MID, 10, 20);

    lv_obj_t *wave_title = lv_label_create(wave_panel);
    lv_obj_set_style_text_color(wave_title, COLOR_TEXT_DIM, 0);
    lv_label_set_text(wave_title, "AUDIO WAVEFORM");
    lv_obj_align(wave_title, LV_ALIGN_TOP_LEFT, 5, 2);

    s_ui.chart_waveform = lv_chart_create(wave_panel);
    lv_obj_set_size(s_ui.chart_waveform, 330, 150);
    lv_obj_align(s_ui.chart_waveform, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_chart_set_type(s_ui.chart_waveform, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_ui.chart_waveform, 128);
    lv_chart_set_range(s_ui.chart_waveform, LV_CHART_AXIS_PRIMARY_Y, -100, 100);
    lv_obj_set_style_bg_color(s_ui.chart_waveform, COLOR_BG_DARK, 0);
    lv_obj_set_style_line_color(s_ui.chart_waveform, COLOR_GRID, LV_PART_MAIN);
    lv_chart_set_div_line_count(s_ui.chart_waveform, 5, 8);

    s_ui.wave_series = lv_chart_add_series(s_ui.chart_waveform, COLOR_ACCENT,
                                            LV_CHART_AXIS_PRIMARY_Y);
    // Initialize with flat line
    for (int i = 0; i < 128; i++) {
        lv_chart_set_next_value(s_ui.chart_waveform, s_ui.wave_series, 0);
    }

    // =============================================
    // Center Panel: CO2 Meter (Analog Gauge)
    // =============================================
    lv_obj_t *co2_panel = lv_obj_create(parent);
    lv_obj_set_size(co2_panel, 250, 250);
    lv_obj_set_style_bg_color(co2_panel, COLOR_BG_PANEL, 0);
    lv_obj_set_style_border_color(co2_panel, COLOR_GRID, 0);
    lv_obj_set_style_border_width(co2_panel, 1, 0);
    lv_obj_set_style_radius(co2_panel, 5, 0);
    lv_obj_set_style_pad_all(co2_panel, 5, 0);
    lv_obj_align(co2_panel, LV_ALIGN_CENTER, 50, 40);

    lv_obj_t *co2_title = lv_label_create(co2_panel);
    lv_obj_set_style_text_color(co2_title, COLOR_TEXT_DIM, 0);
    lv_label_set_text(co2_title, "CO2 LEVEL (ppm)");
    lv_obj_align(co2_title, LV_ALIGN_TOP_MID, 0, 2);

    s_ui.meter_co2 = lv_meter_create(co2_panel);
    lv_obj_set_size(s_ui.meter_co2, 200, 200);
    lv_obj_align(s_ui.meter_co2, LV_ALIGN_CENTER, 0, 15);
    lv_obj_set_style_bg_color(s_ui.meter_co2, COLOR_BG_DARK, 0);

    // Add scale
    lv_meter_scale_t *scale = lv_meter_add_scale(s_ui.meter_co2);
    lv_meter_set_scale_ticks(s_ui.meter_co2, scale, 21, 2, 10, COLOR_TEXT_DIM);
    lv_meter_set_scale_major_ticks(s_ui.meter_co2, scale, 5, 4, 15, COLOR_TEXT, 10);
    lv_meter_set_scale_range(s_ui.meter_co2, scale, 400, 2000, 240, 150);

    // Add colored arcs (Green/Yellow/Red zones)
    lv_meter_indicator_t *arc;
    arc = lv_meter_add_arc(s_ui.meter_co2, scale, 10, lv_color_hex(0x22cc44), -5);
    lv_meter_set_indicator_start_value(s_ui.meter_co2, arc, 400);
    lv_meter_set_indicator_end_value(s_ui.meter_co2, arc, 800);

    arc = lv_meter_add_arc(s_ui.meter_co2, scale, 10, COLOR_WARNING, -5);
    lv_meter_set_indicator_start_value(s_ui.meter_co2, arc, 800);
    lv_meter_set_indicator_end_value(s_ui.meter_co2, arc, 1200);

    arc = lv_meter_add_arc(s_ui.meter_co2, scale, 10, COLOR_DANGER, -5);
    lv_meter_set_indicator_start_value(s_ui.meter_co2, arc, 1200);
    lv_meter_set_indicator_end_value(s_ui.meter_co2, arc, 2000);

    // Add needle
    s_ui.co2_needle = lv_meter_add_needle_line(s_ui.meter_co2, scale, 4, COLOR_ACCENT, -20);
    lv_meter_set_indicator_value(s_ui.meter_co2, s_ui.co2_needle, 450);

    // =============================================
    // Right Panel: Temperature Bar + Stats
    // =============================================
    lv_obj_t *temp_panel = lv_obj_create(parent);
    lv_obj_set_size(temp_panel, 180, 300);
    lv_obj_set_style_bg_color(temp_panel, COLOR_BG_PANEL, 0);
    lv_obj_set_style_border_color(temp_panel, COLOR_GRID, 0);
    lv_obj_set_style_border_width(temp_panel, 1, 0);
    lv_obj_set_style_radius(temp_panel, 5, 0);
    lv_obj_set_style_pad_all(temp_panel, 10, 0);
    lv_obj_align(temp_panel, LV_ALIGN_RIGHT_MID, -10, 20);

    lv_obj_t *temp_title = lv_label_create(temp_panel);
    lv_obj_set_style_text_color(temp_title, COLOR_TEXT_DIM, 0);
    lv_label_set_text(temp_title, "ENVIRONMENT");
    lv_obj_align(temp_title, LV_ALIGN_TOP_MID, 0, 0);

    // Temperature bar (vertical thermometer style)
    s_ui.bar_temp = lv_bar_create(temp_panel);
    lv_obj_set_size(s_ui.bar_temp, 40, 150);
    lv_bar_set_range(s_ui.bar_temp, 0, 40);
    lv_bar_set_value(s_ui.bar_temp, 25, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_ui.bar_temp, COLOR_BG_DARK, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ui.bar_temp, COLOR_ACCENT2, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_ui.bar_temp, 20, 0);
    lv_obj_align(s_ui.bar_temp, LV_ALIGN_LEFT_MID, 20, 0);

    // Temperature value
    s_ui.lbl_temp_value = lv_label_create(temp_panel);
    lv_obj_set_style_text_font(s_ui.lbl_temp_value, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_ui.lbl_temp_value, COLOR_TEXT, 0);
    lv_label_set_text(s_ui.lbl_temp_value, "25.0C");
    lv_obj_align(s_ui.lbl_temp_value, LV_ALIGN_RIGHT_MID, -10, -50);

    // Humidity
    s_ui.lbl_humidity = lv_label_create(temp_panel);
    lv_obj_set_style_text_color(s_ui.lbl_humidity, COLOR_ACCENT2, 0);
    lv_label_set_text(s_ui.lbl_humidity, "50%RH");
    lv_obj_align(s_ui.lbl_humidity, LV_ALIGN_RIGHT_MID, -10, 0);

    // Pressure
    s_ui.lbl_pressure = lv_label_create(temp_panel);
    lv_obj_set_style_text_color(s_ui.lbl_pressure, COLOR_TEXT_DIM, 0);
    lv_label_set_text(s_ui.lbl_pressure, "1013hPa");
    lv_obj_align(s_ui.lbl_pressure, LV_ALIGN_RIGHT_MID, -10, 50);
}

// ============================================================================
// Control Center (Left Tile)
// ============================================================================

static void create_remo_button(lv_obj_t *parent, const char *label_text,
                                const char *app_name, int x, int y)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 140, 50);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, COLOR_BG_PANEL, 0);
    lv_obj_set_style_border_color(btn, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(btn, 1, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label_text);
    lv_obj_center(lbl);

    // Store app_name as user data (static string required)
    lv_obj_add_event_cb(btn, remo_btn_cb, LV_EVENT_CLICKED, (void *)app_name);
}

static void create_control_center(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, COLOR_BG_DARK, 0);
    lv_obj_set_style_pad_all(parent, 0, 0);

    // TabView (Chrome-like tabs)
    s_ui.tabview = lv_tabview_create(parent);
    lv_tabview_set_tab_bar_position(s_ui.tabview, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(s_ui.tabview, 50);
    lv_obj_set_style_bg_color(s_ui.tabview, COLOR_BG_DARK, 0);

    // Style tab buttons
    lv_obj_t *tab_btns = lv_tabview_get_tab_bar(s_ui.tabview);
    lv_obj_set_style_bg_color(tab_btns, COLOR_BG_PANEL, 0);
    lv_obj_set_style_text_color(tab_btns, COLOR_TEXT, 0);

    // Create tabs
    s_ui.tab_lights = lv_tabview_add_tab(s_ui.tabview, "Lights");
    s_ui.tab_climate = lv_tabview_add_tab(s_ui.tabview, "Climate");
    s_ui.tab_media = lv_tabview_add_tab(s_ui.tabview, "Media");

    // Set tab background colors
    lv_obj_set_style_bg_color(s_ui.tab_lights, COLOR_BG_DARK, 0);
    lv_obj_set_style_bg_color(s_ui.tab_climate, COLOR_BG_DARK, 0);
    lv_obj_set_style_bg_color(s_ui.tab_media, COLOR_BG_DARK, 0);

    // ===== Lights Tab =====
    lv_obj_t *lights_title = lv_label_create(s_ui.tab_lights);
    lv_obj_set_style_text_color(lights_title, COLOR_ACCENT, 0);
    lv_label_set_text(lights_title, "LIGHTING CONTROL");
    lv_obj_align(lights_title, LV_ALIGN_TOP_MID, 0, 10);

    create_remo_button(s_ui.tab_lights, "on", "living_room_light", 30, 60);
    create_remo_button(s_ui.tab_lights, "off", "living_room_light", 190, 60);
    create_remo_button(s_ui.tab_lights, "on", "bedroom_light", 30, 130);
    create_remo_button(s_ui.tab_lights, "off", "bedroom_light", 190, 130);

    lv_obj_t *lbl1 = lv_label_create(s_ui.tab_lights);
    lv_label_set_text(lbl1, "Living Room");
    lv_obj_set_style_text_color(lbl1, COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(lbl1, 360, 75);

    lv_obj_t *lbl2 = lv_label_create(s_ui.tab_lights);
    lv_label_set_text(lbl2, "Bedroom");
    lv_obj_set_style_text_color(lbl2, COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(lbl2, 360, 145);

    // ===== Climate Tab =====
    lv_obj_t *climate_title = lv_label_create(s_ui.tab_climate);
    lv_obj_set_style_text_color(climate_title, COLOR_ACCENT, 0);
    lv_label_set_text(climate_title, "CLIMATE CONTROL");
    lv_obj_align(climate_title, LV_ALIGN_TOP_MID, 0, 10);

    create_remo_button(s_ui.tab_climate, "on", "living_room_ac", 30, 60);
    create_remo_button(s_ui.tab_climate, "off", "living_room_ac", 190, 60);
    create_remo_button(s_ui.tab_climate, "cool_25", "living_room_ac", 30, 130);
    create_remo_button(s_ui.tab_climate, "heat_22", "living_room_ac", 190, 130);

    // ===== Media Tab =====
    lv_obj_t *media_title = lv_label_create(s_ui.tab_media);
    lv_obj_set_style_text_color(media_title, COLOR_ACCENT, 0);
    lv_label_set_text(media_title, "MEDIA CONTROL");
    lv_obj_align(media_title, LV_ALIGN_TOP_MID, 0, 10);

    create_remo_button(s_ui.tab_media, "power", "tv", 30, 60);
    create_remo_button(s_ui.tab_media, "vol_up", "tv", 190, 60);
    create_remo_button(s_ui.tab_media, "vol_down", "tv", 350, 60);
}

// ============================================================================
// Extra Tile (Placeholder)
// ============================================================================

static void create_extra_tile(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, COLOR_BG_DARK, 0);

    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl, "Future Expansion");
    lv_obj_center(lbl);

    lv_obj_t *hint = lv_label_create(parent);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_DIM, 0);
    lv_label_set_text(hint, "Swipe left to return");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -50);
}

// ============================================================================
// Public API Implementation
// ============================================================================

int ui_app_init(void)
{
    if (s_ui.initialized) {
        ESP_LOGW(TAG, "UI App already initialized");
        return 0;
    }

    ESP_LOGI(TAG, "Initializing advanced UI...");

    // Create TileView as main container
    s_ui.tileview = lv_tileview_create(lv_screen_active());
    lv_obj_set_size(s_ui.tileview, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_ui.tileview, COLOR_BG_DARK, 0);

    // Create tiles (horizontally scrollable)
    s_ui.tiles[UI_TILE_CONTROL] = lv_tileview_add_tile(s_ui.tileview, 0, 0, LV_DIR_RIGHT);
    s_ui.tiles[UI_TILE_HOME] = lv_tileview_add_tile(s_ui.tileview, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    s_ui.tiles[UI_TILE_EXTRA] = lv_tileview_add_tile(s_ui.tileview, 2, 0, LV_DIR_LEFT);

    // Create content for each tile
    create_control_center(s_ui.tiles[UI_TILE_CONTROL]);
    create_glass_cockpit(s_ui.tiles[UI_TILE_HOME]);
    create_extra_tile(s_ui.tiles[UI_TILE_EXTRA]);

    // Set initial view to Home (center)
    lv_obj_set_tile(s_ui.tileview, s_ui.tiles[UI_TILE_HOME], LV_ANIM_OFF);

    // Create standby overlay
    create_standby_overlay();

    s_ui.initialized = true;
    ESP_LOGI(TAG, "Advanced UI initialized (3-tile TileView + Glass Cockpit)");

    return 0;
}

void ui_app_deinit(void)
{
    if (!s_ui.initialized) return;
    // LVGL handles object cleanup automatically
    memset(&s_ui, 0, sizeof(s_ui));
}

void ui_app_goto_tile(ui_tile_t tile)
{
    if (!s_ui.initialized || tile >= UI_TILE_COUNT) return;
    lv_obj_set_tile(s_ui.tileview, s_ui.tiles[tile], LV_ANIM_ON);
}

void ui_app_set_standby(bool show)
{
    if (!s_ui.initialized || !s_ui.standby_layer) return;

    if (show) {
        lv_obj_clear_flag(s_ui.standby_layer, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_ui.standby_layer, LV_OBJ_FLAG_HIDDEN);
    }
    s_ui.standby_active = show;
}

bool ui_app_is_standby(void)
{
    return s_ui.standby_active;
}

void ui_app_update_time(int hour, int minute, int second)
{
    if (!s_ui.initialized) return;

    char buf[16];

    // Update standby clock
    if (s_ui.standby_clock) {
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour, minute, second);
        lv_label_set_text(s_ui.standby_clock, buf);
    }

    // Update small time on cockpit
    if (s_ui.lbl_time_small) {
        snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);
        lv_label_set_text(s_ui.lbl_time_small, buf);
    }
}

void ui_app_update_sensors(float temp, float humidity, int co2, float pressure)
{
    if (!s_ui.initialized) return;

    char buf[32];

    // Temperature bar and label
    if (s_ui.bar_temp) {
        lv_bar_set_value(s_ui.bar_temp, (int)temp, LV_ANIM_ON);
    }
    if (s_ui.lbl_temp_value) {
        snprintf(buf, sizeof(buf), "%.1fC", temp);
        lv_label_set_text(s_ui.lbl_temp_value, buf);
    }

    // Humidity
    if (s_ui.lbl_humidity) {
        snprintf(buf, sizeof(buf), "%.0f%%RH", humidity);
        lv_label_set_text(s_ui.lbl_humidity, buf);
    }

    // Pressure
    if (s_ui.lbl_pressure) {
        snprintf(buf, sizeof(buf), "%.0fhPa", pressure);
        lv_label_set_text(s_ui.lbl_pressure, buf);
    }

    // CO2 meter needle
    if (s_ui.meter_co2 && s_ui.co2_needle) {
        int display_co2 = co2;
        if (display_co2 < 400) display_co2 = 400;
        if (display_co2 > 2000) display_co2 = 2000;
        lv_meter_set_indicator_value(s_ui.meter_co2, s_ui.co2_needle, display_co2);
    }
}

void ui_app_update_waveform(const int16_t *samples, int count)
{
    if (!s_ui.initialized || !s_ui.chart_waveform || !samples) return;

    // Downsample to 128 points for display
    int step = count / 128;
    if (step < 1) step = 1;

    for (int i = 0; i < 128 && i * step < count; i++) {
        int16_t sample = samples[i * step];
        // Scale to -100..100 range
        int scaled = (sample * 100) / 32767;
        lv_chart_set_next_value(s_ui.chart_waveform, s_ui.wave_series, scaled);
    }

    lv_chart_refresh(s_ui.chart_waveform);
}

void ui_app_refresh_controls(void)
{
    // This would dynamically rebuild buttons from remo_client appliance list
    // For now, the static buttons are sufficient
    ESP_LOGI(TAG, "Control refresh requested");
}
