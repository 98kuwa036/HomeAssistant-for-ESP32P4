idf_component_register(
    SRCS "audio_pipeline.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer freertos esp_psram heap
)

# Audio Pipeline for Omni-P4
#
# Dependencies:
#   - driver: I2S driver for microphone input and DAC output
#
# Audio Flow:
#   I2S1 Microphone (48kHz) -> Dual Buffers -> ESPHome/LLM
#   Output Buffer -> I2S0 -> ES9038Q2M DAC -> Speaker
#
# Supported Microphones:
#   - INMP441: I2S MEMS microphone (mono)
#   - SPH0645: I2S MEMS microphone (mono)
#   - ICS-43434: I2S MEMS microphone (stereo)
#
# Note: USB Audio Class Host is NOT supported on ESP32-P4.
#       The usb_stream component only supports ESP32-S2/S3.
