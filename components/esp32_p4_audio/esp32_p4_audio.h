/**
 * ESP32-P4 Audio Component for ESPHome
 *
 * Provides optimized audio handling for ESP32-P4 with:
 * - ES8311 audio codec support
 * - Dual microphone echo cancellation
 * - PSRAM buffer management
 * - Low-latency audio streaming
 */

#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#ifdef USE_ESP32
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace esphome {
namespace esp32_p4_audio {

static const char *const TAG = "esp32_p4_audio";

class ESP32P4AudioComponent : public Component {
 public:
  ESP32P4AudioComponent() = default;

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  // Configuration setters
  void set_es8311_enabled(bool enabled) { es8311_enabled_ = enabled; }
  void set_echo_cancellation(bool enabled) { echo_cancel_enabled_ = enabled; }
  void set_buffer_size(uint32_t size) { buffer_size_ = size; }
  void set_sample_rate(uint32_t rate) { sample_rate_ = rate; }

  // Audio buffer management
  uint8_t *allocate_audio_buffer(size_t size);
  void free_audio_buffer(uint8_t *buffer);

  // Status
  bool is_ready() const { return initialized_; }
  size_t get_free_psram() const;

 protected:
  bool init_es8311_();
  bool init_echo_cancellation_();
  void configure_audio_clocks_();

  bool es8311_enabled_{true};
  bool echo_cancel_enabled_{false};
  uint32_t buffer_size_{4096};
  uint32_t sample_rate_{16000};
  bool initialized_{false};

  // Audio buffers allocated in PSRAM
  uint8_t *mic_buffer_{nullptr};
  uint8_t *speaker_buffer_{nullptr};
};

}  // namespace esp32_p4_audio
}  // namespace esphome
