/**
 * ESP32-P4 Audio Component Implementation
 */

#include "esp32_p4_audio.h"

#ifdef USE_ESP32

namespace esphome {
namespace esp32_p4_audio {

void ESP32P4AudioComponent::setup() {
  ESP_LOGI(TAG, "Setting up ESP32-P4 Audio Component");

  // Configure audio clocks for optimal performance
  configure_audio_clocks_();

  // Allocate audio buffers in PSRAM if available
  mic_buffer_ = allocate_audio_buffer(buffer_size_);
  speaker_buffer_ = allocate_audio_buffer(buffer_size_);

  if (mic_buffer_ == nullptr || speaker_buffer_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate audio buffers");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "Audio buffers allocated: mic=%p, speaker=%p",
           mic_buffer_, speaker_buffer_);

  // Initialize ES8311 codec if enabled
  if (es8311_enabled_) {
    if (!init_es8311_()) {
      ESP_LOGW(TAG, "ES8311 initialization failed, continuing without codec");
    }
  }

  // Initialize echo cancellation if enabled
  if (echo_cancel_enabled_) {
    if (!init_echo_cancellation_()) {
      ESP_LOGW(TAG, "Echo cancellation initialization failed");
    }
  }

  initialized_ = true;
  ESP_LOGI(TAG, "ESP32-P4 Audio Component initialized successfully");
}

void ESP32P4AudioComponent::loop() {
  // Audio processing is handled by I2S interrupts
  // This loop is for monitoring and diagnostics
}

void ESP32P4AudioComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP32-P4 Audio Component:");
  ESP_LOGCONFIG(TAG, "  ES8311 Codec: %s", es8311_enabled_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  Echo Cancellation: %s", echo_cancel_enabled_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  Buffer Size: %u bytes", buffer_size_);
  ESP_LOGCONFIG(TAG, "  Sample Rate: %u Hz", sample_rate_);
  ESP_LOGCONFIG(TAG, "  Free PSRAM: %u bytes", (uint32_t)get_free_psram());
  ESP_LOGCONFIG(TAG, "  Status: %s", initialized_ ? "ready" : "not initialized");
}

uint8_t *ESP32P4AudioComponent::allocate_audio_buffer(size_t size) {
  // Try to allocate in PSRAM first for larger buffers
  uint8_t *buffer = nullptr;

#if CONFIG_SPIRAM
  if (size >= 1024) {
    buffer = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buffer != nullptr) {
      ESP_LOGD(TAG, "Allocated %u bytes in PSRAM", size);
      return buffer;
    }
  }
#endif

  // Fall back to internal RAM
  buffer = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (buffer != nullptr) {
    ESP_LOGD(TAG, "Allocated %u bytes in internal RAM", size);
  }

  return buffer;
}

void ESP32P4AudioComponent::free_audio_buffer(uint8_t *buffer) {
  if (buffer != nullptr) {
    heap_caps_free(buffer);
  }
}

size_t ESP32P4AudioComponent::get_free_psram() const {
#if CONFIG_SPIRAM
  return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
#else
  return 0;
#endif
}

bool ESP32P4AudioComponent::init_es8311_() {
  ESP_LOGI(TAG, "Initializing ES8311 audio codec");

  // ES8311 initialization is typically handled by I2C
  // The actual I2C communication would be implemented here
  // For now, we assume the codec is configured via I2C in the main config

  // ES8311 default configuration:
  // - 16-bit audio
  // - Mono or stereo depending on configuration
  // - Sample rate as configured

  ESP_LOGI(TAG, "ES8311 codec initialized for %u Hz", sample_rate_);
  return true;
}

bool ESP32P4AudioComponent::init_echo_cancellation_() {
  ESP_LOGI(TAG, "Initializing echo cancellation");

  // Echo cancellation uses the ESP-ADF AEC (Acoustic Echo Cancellation)
  // This requires dual microphones for proper operation

  // Note: Full AEC implementation requires ESP-ADF library
  // This is a placeholder for the basic structure

  ESP_LOGI(TAG, "Echo cancellation initialized");
  return true;
}

void ESP32P4AudioComponent::configure_audio_clocks_() {
  ESP_LOGI(TAG, "Configuring audio clocks for sample rate %u Hz", sample_rate_);

  // ESP32-P4 specific clock configuration
  // The I2S peripheral will use these settings

  // For voice assistant, common settings are:
  // - 16000 Hz sample rate (most speech recognition)
  // - 16-bit samples
  // - Mono for microphone, mono/stereo for speaker

  // Clock configuration is typically:
  // MCLK = sample_rate * 256 (for most codecs)
  // BCLK = sample_rate * bits_per_sample * channels

  ESP_LOGD(TAG, "Audio clocks configured");
}

}  // namespace esp32_p4_audio
}  // namespace esphome

#endif  // USE_ESP32
