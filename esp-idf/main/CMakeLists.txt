# Omni-P4 v4.1 (B&W Trace) - Main Component
#
# Hardware:
#   - ReSpeaker USB Mic Array v2.0 (USB Host UAC)
#   - ES9039Q2M DAC (I2S0)
#   - 7" MIPI-DSI LCD (1024x600)
#   - 8 Sensor Modules (I2C + UART)

idf_component_register(
    SRCS
        "main.c"
        "usb_audio.c"
        "i2s_output.c"
        "wifi_manager.c"
        "ha_client.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        driver
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_uart
        esp_driver_i2s
        esp_wifi
        esp_event
        nvs_flash
        usb_host_uac
        esp_http_client
        json
        esp_tls
        esp_lcd
)
