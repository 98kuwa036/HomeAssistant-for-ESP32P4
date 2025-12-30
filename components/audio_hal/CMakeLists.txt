idf_component_register(
    SRCS "audio_pipeline.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer freertos esp_psram heap usb_audio_host
)

# Audio Pipeline for Omni-P4
#
# Dependencies:
#   - usb_audio_host: USB Audio Class host for ReSpeaker USB Mic Array
#   - driver: I2S driver for ES9038Q2M DAC output
#
# Audio Flow:
#   ReSpeaker USB (48kHz) -> USB Host -> Dual Buffers -> ESPHome/LLM
#   Output Buffer -> I2S0 -> ES9038Q2M DAC -> Speaker
