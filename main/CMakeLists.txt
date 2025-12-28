idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        freertos
        esp_event
        esp_timer
        nvs_flash
        esp_psram
        sensor_hub
        audio_hal
        led_effect
        display_manager
        command_cache
)
