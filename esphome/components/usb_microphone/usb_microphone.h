/**
 * @file usb_microphone.h
 * @brief ESPHome USB Microphone component for ReSpeaker USB Mic Array
 *
 * This component wraps the usb_audio_input ESP-IDF component to provide
 * a microphone interface compatible with ESPHome's voice_assistant.
 */

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/microphone/microphone.h"

#ifdef USE_ESP_IDF
extern "C" {
#include "usb_audio_input.h"
#include "audio_pipeline.h"
}
#endif

namespace esphome {
namespace usb_microphone {

class USBMicrophone : public microphone::Microphone, public Component {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void start() override;
  void stop() override;

  size_t read(int16_t *buf, size_t len) override;

 protected:
  bool is_running_{false};
  bool usb_initialized_{false};
};

}  // namespace usb_microphone
}  // namespace esphome
