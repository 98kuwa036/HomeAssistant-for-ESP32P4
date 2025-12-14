# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0-beta] - 2025-12-13

### Added

- Initial ESP32-P4 Voice Assistant implementation
- ESPHome configuration files for multiple boards:
  - ESP32-P4-Function-EV-Board
  - Waveshare ESP32-P4-NANO
  - Smart 86 Box 4" Panel
  - Minimal/Generic configuration
- Custom ESPHome components:
  - `esp32_p4_audio`: Audio buffer management and optimization
  - `es8311`: ES8311 audio codec support
- Common configuration modules:
  - Base configuration
  - WiFi configuration
  - Voice assistant configuration
- Documentation:
  - Home Assistant setup guide
  - Hardware guide with pin configurations
  - README with quick start instructions

### Known Limitations (Beta)

- ESP32-P4 support in ESPHome requires ESP-IDF framework (Arduino not supported)
- Bluetooth functionality requires external ESP32-C6 companion chip
- Some audio codecs may require additional configuration
- Display integration is not yet implemented
- Echo cancellation is experimental

### Requirements

- ESPHome 2025.6.0 or later
- Home Assistant 2023.5 or later
- ESP-IDF 5.3.1 or later

## Future Plans

### [1.1.0] - Planned

- Display UI for voice assistant status
- Additional wake word models
- Improved echo cancellation
- Multi-room audio support

### [1.2.0] - Planned

- Local speech recognition (Whisper on device)
- Camera integration for visual wake
- Gesture control support
