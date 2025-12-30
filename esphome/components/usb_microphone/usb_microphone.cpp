/**
 * @file usb_microphone.cpp
 * @brief ESPHome USB Microphone implementation
 *
 * Integrates ReSpeaker USB Mic Array with ESPHome voice_assistant
 * via the usb_audio_input and audio_pipeline ESP-IDF components.
 */

#include "usb_microphone.h"
#include "esphome/core/log.h"

namespace esphome {
namespace usb_microphone {

static const char *const TAG = "usb_microphone";

void USBMicrophone::setup() {
  ESP_LOGCONFIG(TAG, "Setting up USB Microphone...");

#ifdef USE_ESP_IDF
  // Initialize audio pipeline (handles USB audio init internally)
  esp_err_t err = audio_pipeline_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize audio pipeline: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  this->usb_initialized_ = true;
  ESP_LOGI(TAG, "USB Microphone initialized successfully");
#else
  ESP_LOGE(TAG, "USB Microphone requires ESP-IDF framework");
  this->mark_failed();
#endif
}

void USBMicrophone::loop() {
  // Nothing to do in loop - audio is handled by callbacks
}

void USBMicrophone::start() {
  if (!this->usb_initialized_) {
    ESP_LOGW(TAG, "USB Microphone not initialized");
    return;
  }

  if (this->is_running_) {
    return;
  }

#ifdef USE_ESP_IDF
  ESP_LOGI(TAG, "Starting USB Microphone...");

  // Start audio recording (enables USB audio streaming)
  esp_err_t err = audio_pipeline_record_start();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start audio recording: %s", esp_err_to_name(err));
    return;
  }

  this->is_running_ = true;
  this->state_ = microphone::STATE_RUNNING;
  ESP_LOGI(TAG, "USB Microphone started");
#endif
}

void USBMicrophone::stop() {
  if (!this->is_running_) {
    return;
  }

#ifdef USE_ESP_IDF
  ESP_LOGI(TAG, "Stopping USB Microphone...");

  audio_pipeline_record_stop();

  this->is_running_ = false;
  this->state_ = microphone::STATE_STOPPED;
  ESP_LOGI(TAG, "USB Microphone stopped");
#endif
}

size_t USBMicrophone::read(int16_t *buf, size_t len) {
  if (!this->is_running_) {
    return 0;
  }

#ifdef USE_ESP_IDF
  // Read processed (16kHz mono) audio from pipeline buffer
  // audio_pipeline_read returns processed audio suitable for ESPHome
  size_t bytes_read = audio_pipeline_read((uint8_t *)buf, len * sizeof(int16_t), 100);

  return bytes_read / sizeof(int16_t);
#else
  return 0;
#endif
}

}  // namespace usb_microphone
}  // namespace esphome
