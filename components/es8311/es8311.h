/**
 * ES8311 Audio Codec Component
 *
 * I2C-controlled low-power mono audio codec commonly used
 * with ESP32-P4 development boards.
 *
 * Register map based on ES8311 datasheet.
 */

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace es8311 {

// ES8311 Register Addresses
enum ES8311Register : uint8_t {
  ES8311_REG_RESET = 0x00,
  ES8311_REG_CLK_MANAGER1 = 0x01,
  ES8311_REG_CLK_MANAGER2 = 0x02,
  ES8311_REG_CLK_MANAGER3 = 0x03,
  ES8311_REG_CLK_MANAGER4 = 0x04,
  ES8311_REG_CLK_MANAGER5 = 0x05,
  ES8311_REG_CLK_MANAGER6 = 0x06,
  ES8311_REG_CLK_MANAGER7 = 0x07,
  ES8311_REG_CLK_MANAGER8 = 0x08,
  ES8311_REG_SDPIN = 0x09,
  ES8311_REG_SDPOUT = 0x0A,
  ES8311_REG_SYSTEM = 0x0B,
  ES8311_REG_SYSTEM2 = 0x0C,
  ES8311_REG_SDP_MISC = 0x0D,
  ES8311_REG_ADC_CTRL1 = 0x0E,
  ES8311_REG_ADC_CTRL2 = 0x0F,
  ES8311_REG_DAC_CTRL1 = 0x10,
  ES8311_REG_DAC_CTRL2 = 0x11,
  ES8311_REG_DAC_CTRL3 = 0x12,
  ES8311_REG_ADC_VOL = 0x13,
  ES8311_REG_DAC_VOL = 0x14,
  ES8311_REG_GPIO = 0x15,
  ES8311_REG_GP = 0x16,
  ES8311_REG_ADC_RAMPRATE = 0x17,
  ES8311_REG_DAC_RAMPRATE = 0x18,
};

static const char *const TAG = "es8311";

class ES8311Component : public Component, public i2c::I2CDevice {
 public:
  ES8311Component() = default;

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  // Configuration
  void set_sample_rate(uint32_t rate) { sample_rate_ = rate; }
  void set_mic_gain(uint8_t gain) { mic_gain_ = gain; }
  void set_speaker_volume(uint8_t volume) { speaker_volume_ = volume; }
  void set_dac_enabled(bool enabled) { dac_enabled_ = enabled; }
  void set_adc_enabled(bool enabled) { adc_enabled_ = enabled; }

  // Runtime control
  void set_volume(uint8_t volume);
  void set_mute(bool mute);
  void set_microphone_gain(uint8_t gain);

 protected:
  bool write_register_(uint8_t reg, uint8_t value);
  uint8_t read_register_(uint8_t reg);
  bool init_codec_();
  bool configure_clocks_();
  bool configure_adc_();
  bool configure_dac_();

  uint32_t sample_rate_{16000};
  uint8_t mic_gain_{24};
  uint8_t speaker_volume_{200};
  bool dac_enabled_{true};
  bool adc_enabled_{true};
  bool initialized_{false};
};

}  // namespace es8311
}  // namespace esphome
