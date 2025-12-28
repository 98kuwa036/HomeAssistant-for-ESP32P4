idf_component_register(
    SRCS "audio_pipeline.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer freertos esp_psram heap
)
