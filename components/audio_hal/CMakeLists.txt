# Audio Pipeline Component for Omni-P4
#
# Supports two microphone input modes:
#   1. USB Audio (CONFIG_AUDIO_INPUT_USB) - ReSpeaker USB Mic Array, UAC 1.0
#   2. I2S Audio (default) - INMP441, SPH0645, ICS-43434
#
# Audio Flow:
#   [USB/I2S] Microphone -> Dual Buffers -> ESPHome/LLM
#   Output Buffer -> I2S0 -> ES9038Q2M DAC -> Speaker

set(REQUIRES driver esp_timer freertos esp_psram heap)

# Add usb_audio_input dependency when USB audio is enabled
if(CONFIG_AUDIO_INPUT_USB)
    list(APPEND REQUIRES usb_audio_input)
endif()

idf_component_register(
    SRCS "audio_pipeline.c"
    INCLUDE_DIRS "."
    REQUIRES ${REQUIRES}
)
