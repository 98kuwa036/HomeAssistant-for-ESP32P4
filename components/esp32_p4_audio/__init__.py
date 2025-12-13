"""
ESP32-P4 Audio Component for ESPHome
Custom audio handling optimized for ESP32-P4's audio capabilities

This component provides:
- ES8311 audio codec support
- Dual microphone echo cancellation
- Audio buffer management for PSRAM
- Low-latency audio streaming
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["i2c"]
MULTI_CONF = True

CONF_ES8311 = "es8311"
CONF_ECHO_CANCEL = "echo_cancellation"
CONF_BUFFER_SIZE = "buffer_size"
CONF_SAMPLE_RATE = "sample_rate"

esp32_p4_audio_ns = cg.esphome_ns.namespace("esp32_p4_audio")
ESP32P4AudioComponent = esp32_p4_audio_ns.class_(
    "ESP32P4AudioComponent", cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESP32P4AudioComponent),
        cv.Optional(CONF_ES8311, default=True): cv.boolean,
        cv.Optional(CONF_ECHO_CANCEL, default=False): cv.boolean,
        cv.Optional(CONF_BUFFER_SIZE, default=4096): cv.int_range(min=512, max=32768),
        cv.Optional(CONF_SAMPLE_RATE, default=16000): cv.one_of(8000, 16000, 22050, 44100, 48000),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_es8311_enabled(config[CONF_ES8311]))
    cg.add(var.set_echo_cancellation(config[CONF_ECHO_CANCEL]))
    cg.add(var.set_buffer_size(config[CONF_BUFFER_SIZE]))
    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))
