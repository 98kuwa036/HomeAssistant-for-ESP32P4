/**
 * ES8311 Audio Codec Implementation
 */

#include "es8311.h"
#include "esphome/core/log.h"

namespace esphome {
namespace es8311 {

void ES8311Component::setup() {
  ESP_LOGI(TAG, "Setting up ES8311 Audio Codec");

  // Reset the codec
  if (!write_register_(ES8311_REG_RESET, 0x3F)) {
    ESP_LOGE(TAG, "Failed to reset ES8311");
    this->mark_failed();
    return;
  }
  delay(10);

  // Exit reset
  if (!write_register_(ES8311_REG_RESET, 0x00)) {
    ESP_LOGE(TAG, "Failed to exit reset");
    this->mark_failed();
    return;
  }
  delay(10);

  // Initialize codec
  if (!init_codec_()) {
    ESP_LOGE(TAG, "Failed to initialize ES8311");
    this->mark_failed();
    return;
  }

  initialized_ = true;
  ESP_LOGI(TAG, "ES8311 Audio Codec initialized successfully");
}

void ES8311Component::dump_config() {
  ESP_LOGCONFIG(TAG, "ES8311 Audio Codec:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Sample Rate: %u Hz", sample_rate_);
  ESP_LOGCONFIG(TAG, "  Microphone Gain: %u dB", mic_gain_);
  ESP_LOGCONFIG(TAG, "  Speaker Volume: %u", speaker_volume_);
  ESP_LOGCONFIG(TAG, "  DAC: %s", dac_enabled_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  ADC: %s", adc_enabled_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  Status: %s", initialized_ ? "ready" : "failed");
}

bool ES8311Component::write_register_(uint8_t reg, uint8_t value) {
  return this->write_byte(reg, value);
}

uint8_t ES8311Component::read_register_(uint8_t reg) {
  uint8_t value = 0;
  this->read_byte(reg, &value);
  return value;
}

bool ES8311Component::init_codec_() {
  // Configure clocks
  if (!configure_clocks_()) {
    return false;
  }

  // Configure ADC (microphone input)
  if (adc_enabled_ && !configure_adc_()) {
    return false;
  }

  // Configure DAC (speaker output)
  if (dac_enabled_ && !configure_dac_()) {
    return false;
  }

  return true;
}

bool ES8311Component::configure_clocks_() {
  ESP_LOGD(TAG, "Configuring clocks for %u Hz", sample_rate_);

  // Clock manager configuration
  // These values depend on MCLK and desired sample rate

  // CLK_MANAGER1: Enable MCLK input
  write_register_(ES8311_REG_CLK_MANAGER1, 0x3F);

  // CLK_MANAGER2: MCLK/BCLK ratio and dividers
  // For 16kHz with 256*Fs MCLK
  uint8_t clk2_val = 0x00;
  switch (sample_rate_) {
    case 8000:
      clk2_val = 0x02;  // Higher divider
      break;
    case 16000:
      clk2_val = 0x01;  // Standard divider
      break;
    case 44100:
    case 48000:
      clk2_val = 0x00;  // Lower divider
      break;
    default:
      clk2_val = 0x01;
  }
  write_register_(ES8311_REG_CLK_MANAGER2, clk2_val);

  // System clock configuration
  write_register_(ES8311_REG_SYSTEM, 0x00);

  return true;
}

bool ES8311Component::configure_adc_() {
  ESP_LOGD(TAG, "Configuring ADC with gain %u dB", mic_gain_);

  // ADC Control 1: Power up ADC, set PGA gain
  uint8_t adc_ctrl1 = 0x00;  // ADC power on
  write_register_(ES8311_REG_ADC_CTRL1, adc_ctrl1);

  // ADC Control 2: Configure ADC scaling and HPF
  write_register_(ES8311_REG_ADC_CTRL2, 0x00);

  // Set ADC volume (microphone gain)
  // Range: 0-255, 192 = 0dB
  uint8_t adc_vol = 192 + (mic_gain_ * 2);  // Convert dB to register value
  if (adc_vol > 255) adc_vol = 255;
  write_register_(ES8311_REG_ADC_VOL, adc_vol);

  // SDP Input configuration for I2S
  write_register_(ES8311_REG_SDPIN, 0x0C);  // 16-bit, I2S format

  return true;
}

bool ES8311Component::configure_dac_() {
  ESP_LOGD(TAG, "Configuring DAC with volume %u", speaker_volume_);

  // DAC Control 1: Power up DAC
  write_register_(ES8311_REG_DAC_CTRL1, 0x00);

  // DAC Control 2: DAC scaling
  write_register_(ES8311_REG_DAC_CTRL2, 0x00);

  // DAC Control 3: Output configuration
  write_register_(ES8311_REG_DAC_CTRL3, 0x00);

  // Set DAC volume
  write_register_(ES8311_REG_DAC_VOL, speaker_volume_);

  // SDP Output configuration for I2S
  write_register_(ES8311_REG_SDPOUT, 0x0C);  // 16-bit, I2S format

  return true;
}

void ES8311Component::set_volume(uint8_t volume) {
  if (!initialized_) return;

  speaker_volume_ = volume;
  write_register_(ES8311_REG_DAC_VOL, volume);
  ESP_LOGD(TAG, "Volume set to %u", volume);
}

void ES8311Component::set_mute(bool mute) {
  if (!initialized_) return;

  if (mute) {
    write_register_(ES8311_REG_DAC_VOL, 0);
  } else {
    write_register_(ES8311_REG_DAC_VOL, speaker_volume_);
  }
  ESP_LOGD(TAG, "Mute: %s", mute ? "on" : "off");
}

void ES8311Component::set_microphone_gain(uint8_t gain) {
  if (!initialized_) return;

  mic_gain_ = gain;
  uint8_t adc_vol = 192 + (gain * 2);
  if (adc_vol > 255) adc_vol = 255;
  write_register_(ES8311_REG_ADC_VOL, adc_vol);
  ESP_LOGD(TAG, "Microphone gain set to %u dB", gain);
}

}  // namespace es8311
}  // namespace esphome
