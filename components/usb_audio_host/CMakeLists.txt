idf_component_register(
    SRCS "usb_audio_host.c"
    INCLUDE_DIRS "."
    REQUIRES driver usb_host freertos heap
    PRIV_REQUIRES esp_timer esp_psram
)

# USB Audio Class Host for ReSpeaker USB Mic Array
#
# Supported devices:
#   - ReSpeaker USB Mic Array v2.0 (VID: 0x2886, PID: 0x0018)
#   - Other USB Audio Class 1.0 compatible microphones
#
# ESP-IDF Version: 5.3+
# Target: ESP32-P4
