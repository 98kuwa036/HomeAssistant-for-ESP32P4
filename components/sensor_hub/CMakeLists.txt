idf_component_register(
    SRCS "sensor_hub.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer freertos
)
