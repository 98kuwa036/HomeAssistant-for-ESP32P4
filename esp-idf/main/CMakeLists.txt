idf_component_register(
    SRCS
        "main.c"
        "usb_audio.c"
        "i2s_output.c"
        "wifi_manager.c"
        "ha_client.c"
        "sensor_hub.c"
        # I2C Sensors
        "drivers/scd41.c"
        "drivers/sgp40.c"
        "drivers/ens160.c"
        "drivers/bmp388.c"
        "drivers/sht40.c"
        "drivers/sen0428.c"
        # UART Sensors
        "drivers/sen0395.c"
        "drivers/sen0540.c"
        # Audio
        "drivers/es9039.c"
        "drivers/xvf3800.c"
    INCLUDE_DIRS
        "."
        "drivers"
    REQUIRES
        driver
        esp_wifi
        esp_event
        nvs_flash
        usb_host_uac
        esp_http_client
        json
        esp_tls
)
