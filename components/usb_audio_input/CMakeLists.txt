idf_component_register(
    SRCS "usb_audio_input.c"
    INCLUDE_DIRS "."
    REQUIRES freertos usb_host
)

# USB Audio Input Component
#
# Provides USB Host Audio Class (UAC 1.0) support for ESP32-P4
# using the espressif/usb_host_uac component.
#
# Supported devices:
#   - ReSpeaker USB Mic Array v2.0 (VID: 0x2886, PID: 0x0018)
#   - Any UAC 1.0 compatible USB microphone
#
# Dependencies (via idf_component.yml):
#   - espressif/usb_host_uac: ^1.3.0
