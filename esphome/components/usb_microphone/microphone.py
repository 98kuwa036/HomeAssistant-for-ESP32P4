# USB Microphone platform for ESPHome
# Provides microphone interface for ReSpeaker USB Mic Array

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import microphone
from esphome.const import CONF_ID
from . import usb_microphone_ns

DEPENDENCIES = ["microphone"]
AUTO_LOAD = []

# Specify this component only works with ESP-IDF
CODEOWNERS = ["@98kuwa036"]

USBMicrophone = usb_microphone_ns.class_(
    "USBMicrophone", microphone.Microphone, cg.Component
)

CONFIG_SCHEMA = cv.All(
    microphone.MICROPHONE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(USBMicrophone),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_esp_idf,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await microphone.register_microphone(var, config)

    # Add ESP-IDF component include paths
    cg.add_build_flag("-I../../../components/usb_audio_input")
    cg.add_build_flag("-I../../../components/audio_hal")
