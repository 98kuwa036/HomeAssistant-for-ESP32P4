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
        remo_client
)

# Create SPIFFS partition image from spiffs_image folder
spiffs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)
