# USB Microphone ESPHome Component
# Wrapper for usb_audio_input ESP-IDF component

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@98kuwa036"]
DEPENDENCIES = []

usb_microphone_ns = cg.esphome_ns.namespace("usb_microphone")

CONF_USB_MICROPHONE_ID = "usb_microphone_id"
