idf_component_register(
    SRCS "display_manager.c" "ui/ui_app.c"
    INCLUDE_DIRS "." "ui"
    REQUIRES driver esp_timer esp_lcd freertos esp_psram sensor_hub lvgl remo_client
)
