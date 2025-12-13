"""
ES8311 Audio Codec Component for ESPHome
Support for the ES8311 low-power mono audio codec used in ESP32-P4 boards

Features:
- I2C control interface
- 8kHz to 96kHz sample rates
- High SNR (Signal-to-Noise Ratio)
- Low power consumption
- Integrated headphone amplifier
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@homeassistant"]

CONF_SAMPLE_RATE = "sample_rate"
CONF_MIC_GAIN = "mic_gain"
CONF_SPEAKER_VOLUME = "speaker_volume"
CONF_DAC_ENABLED = "dac_enabled"
CONF_ADC_ENABLED = "adc_enabled"

es8311_ns = cg.esphome_ns.namespace("es8311")
ES8311Component = es8311_ns.class_(
    "ES8311Component", cg.Component, i2c.I2CDevice
)

# Sample rates supported by ES8311
SAMPLE_RATES = [8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 88200, 96000]

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ES8311Component),
            cv.Optional(CONF_SAMPLE_RATE, default=16000): cv.one_of(*SAMPLE_RATES, int=True),
            cv.Optional(CONF_MIC_GAIN, default=24): cv.int_range(min=0, max=42),
            cv.Optional(CONF_SPEAKER_VOLUME, default=200): cv.int_range(min=0, max=255),
            cv.Optional(CONF_DAC_ENABLED, default=True): cv.boolean,
            cv.Optional(CONF_ADC_ENABLED, default=True): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x18))  # Default I2C address for ES8311
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))
    cg.add(var.set_mic_gain(config[CONF_MIC_GAIN]))
    cg.add(var.set_speaker_volume(config[CONF_SPEAKER_VOLUME]))
    cg.add(var.set_dac_enabled(config[CONF_DAC_ENABLED]))
    cg.add(var.set_adc_enabled(config[CONF_ADC_ENABLED]))
