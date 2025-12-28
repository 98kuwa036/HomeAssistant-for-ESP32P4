idf_component_register(
    SRCS "remo_client.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client mdns spiffs json freertos
)
