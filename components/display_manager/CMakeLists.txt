idf_component_register(
    SRCS "display_manager.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer esp_lcd freertos esp_psram sensor_hub lvgl
)
