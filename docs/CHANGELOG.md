# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2.0-beta] - 2025-12-16

### Added

- `esp32p4-respeaker-smart-speaker.yaml`: ReSpeaker USB Mic Array 構成
  - ReSpeaker USB を Raspberry Pi に接続
  - PAM8403 アンプ + Peerless スピーカー対応
  - ES8311 DAC 出力を PAM8403 に接続
  - 環境センサー (SCD40, SPS30) 対応
- `docs/respeaker-setup.md`: ReSpeaker セットアップガイド
  - Wyoming Satellite 設定手順
  - Assist Pipeline 構成
  - ハードウェア接続図

### Changed

- README の設定ファイル一覧にマイク列を追加
- ドキュメントリンクに ReSpeaker ガイドを追加

## [1.1.0-beta] - 2025-12-15

### Added

- `esp32p4-smart-speaker.yaml`: 高機能スマートスピーカー構成
  - SCD40センサー (CO2、温度、湿度)
  - SPS30センサー (PM1.0, PM2.5, PM4.0, PM10)
  - 空気質指数 (AQI) 自動計算
  - 空気質ステータステキストセンサー
  - SPS30ファンクリーニングボタン
  - MIPIカメラ対応準備（ESPHomeサポート待ち）
- README に利用可能な設定ファイル一覧を追加

### Changed

- README の機能一覧に環境センサーを追加
- プロジェクト構成の説明を更新

## [1.0.1-beta] - 2025-12-14

### Added

- Raspberry Pi 4B + Google AI Studio 構成ガイド
- `esp32p4-lightweight.yaml`: クラウドオフロード向け軽量設定
- Google Cloud STT/TTS 統合ドキュメント
- Gemma 3 / Gemini API 設定手順

### Changed

- README に推奨構成（Raspberry Pi 4B + Google AI）を追加
- ドキュメント構成を更新

### Fixed

- `esp32p4-voice-assistant.yaml`: `on_client_connected` インデント修正

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
