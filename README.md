# Home Assistant Voice Assistant for ESP32-P4-Function-EV-Board

ESP32-P4-Function-EV-Board を使用した Home Assistant 音声アシスタント実装です。
Raspberry Pi 4B + Google AI Studio (Gemma 3) のクラウドオフロード構成に最適化されています。

> **Beta Version**: ESP32-P4のESPHomeサポートは2025年6月から開始されており、一部の機能はまだ開発中です。

## Target Hardware

| コンポーネント | 詳細 |
|--------------|------|
| **開発ボード** | [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/) v1.5 |
| **CPU** | ESP32-P4 Dual-core RISC-V @ 400MHz |
| **Audio Codec** | ES8311 + NS4150B Amplifier |
| **WiFi/BLE** | ESP32-C6-MINI-1 (WiFi 6) |
| **Memory** | 32MB PSRAM, 16MB Flash |

## Features

- **Voice Control**: Home Assistantと連携した音声アシスタント
- **Wake Word**: オンデバイス検出（"Okay Nabu"）
- **ES8311 Codec**: 高品質オーディオ入出力
- **Cloud Offload**: STT/TTS/LLMをGoogle Cloudで処理
- **Japanese Support**: 日本語音声対応

## System Architecture

```
┌──────────────────────────────┐
│  ESP32-P4-Function-EV-Board  │
│  ┌────────┐    ┌──────────┐  │
│  │ES8311  │    │ESP32-C6  │  │
│  │Codec   │    │WiFi 6    │  │
│  └────────┘    └──────────┘  │
└──────────────┬───────────────┘
               │ WiFi
               ▼
┌──────────────────────────────┐
│     Raspberry Pi 4B          │
│     Home Assistant           │
└──────────────┬───────────────┘
               │ Internet
               ▼
┌──────────────────────────────┐
│      Google Cloud Services   │
│  - AI Studio (Gemma 3)       │
│  - Cloud Speech-to-Text      │
│  - Cloud Text-to-Speech      │
└──────────────────────────────┘
```

## Pin Configuration

| 機能 | GPIO | 説明 |
|------|------|------|
| I2S MCLK | GPIO13 | Master Clock |
| I2S BCLK | GPIO12 | Bit Clock |
| I2S WS | GPIO10 | Word Select |
| I2S DOUT | GPIO9 | Speaker Data |
| I2S DIN | GPIO11 | Microphone Data |
| I2C SDA | GPIO7 | ES8311 Control |
| I2C SCL | GPIO8 | ES8311 Control |
| LED | GPIO47 | Status (WS2812) |
| Button | GPIO0 | Boot Button |

## Requirements

- **ESPHome** 2025.6.0以降
- **Home Assistant** 2023.5以降
- **Raspberry Pi 4B** (4GB以上推奨)
- **Google AI Studio** APIキー
- **Google Cloud** STT/TTS API

## Quick Start

### 1. Clone & Configure

```bash
git clone https://github.com/98kuwa036/HomeAssistant-for-ESP32P4.git
cd HomeAssistant-for-ESP32P4

# secrets設定
cp esphome/secrets.yaml.example esphome/secrets.yaml
```

`secrets.yaml` を編集：
```yaml
wifi_ssid: "YourWiFiSSID"
wifi_password: "YourWiFiPassword"
ap_password: "FallbackPassword"
api_encryption_key: "YOUR_32_CHAR_KEY"  # esphome generate-encryption-key
ota_password: "YourOTAPassword"
```

### 2. Flash Device

```bash
pip install esphome
esphome run esphome/configs/esp32p4-function-ev-board.yaml
```

### 3. Configure Home Assistant

[Raspberry Pi 4B + Google AI Studio 構成ガイド](docs/raspberry-pi-setup.md) を参照してください。

## Project Structure

```
HomeAssistant-for-ESP32P4/
├── esphome/
│   ├── configs/
│   │   └── esp32p4-function-ev-board.yaml  # メイン設定
│   ├── common/
│   │   ├── base.yaml
│   │   ├── wifi.yaml
│   │   └── voice.yaml
│   └── secrets.yaml.example
├── components/
│   ├── esp32_p4_audio/       # P4オーディオ最適化
│   └── es8311/               # ES8311コーデックドライバ
├── docs/
│   ├── raspberry-pi-setup.md # RPi 4B + Google AI ガイド
│   ├── hardware-guide.md
│   └── CHANGELOG.md
└── README.md
```

## LED Status

| 色 | 状態 |
|----|------|
| 青 | リスニング中 |
| シアン | 音声検出 |
| 黄 | 処理中 |
| 紫 | 応答中 |
| 赤 | エラー |
| 緑 | 接続完了 |

## Documentation

- **[Raspberry Pi 4B + Google AI Setup](docs/raspberry-pi-setup.md)** - 推奨構成の詳細ガイド
- [Hardware Guide](docs/hardware-guide.md) - ハードウェア詳細
- [Changelog](docs/CHANGELOG.md) - 変更履歴

## Troubleshooting

### 音声が認識されない
1. マイクが正しく接続されているか確認
2. `noise_suppression_level` を調整（0-4）
3. シリアルログを確認: `esphome logs`

### 音声出力がない
1. スピーカー接続を確認
2. `volume_multiplier` を増加（1.0-3.0）
3. ES8311のI2C通信を確認

### WiFi接続が不安定
1. `power_save_mode: none` を確認
2. ESP32-C6のアンテナ位置を調整
3. `output_power: 20dB` に設定

## References

- [ESP32-P4-Function-EV-Board User Guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/user_guide.html)
- [ESPHome Voice Assistant](https://esphome.io/components/voice_assistant.html)
- [Google AI Studio](https://aistudio.google.com/)
- [Google Cloud Speech-to-Text](https://cloud.google.com/speech-to-text)

## License

Apache License 2.0 - see [LICENSE](LICENSE)
