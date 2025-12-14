# Home Assistant Voice Assistant for ESP32-P4

ESP32-P4ベースのHome Assistant音声アシスタント実装です。ESPHomeを使用して、ESP32-P4の高性能オーディオ機能を活用した音声コントロールを実現します。

> **Beta Version**: このプロジェクトは現在ベータ版です。ESP32-P4のESPHomeサポートは2025年6月から開始されており、一部の機能はまだ開発中です。

## Features

- **Voice Control**: Home Assistantと連携した音声アシスタント機能
- **Wake Word Detection**: オンデバイスのウェイクワード検出（"Okay Nabu"など）
- **Low Latency**: ESP32-P4の400MHz RISC-Vプロセッサによる高速処理
- **Multiple Boards**: 複数の開発ボードに対応した設定ファイル
- **ES8311 Codec**: 高品質オーディオコーデックのサポート

## Supported Hardware

| Board | Status | Features |
|-------|--------|----------|
| ESP32-P4-Function-EV-Board | Tested | ES8311 codec, dual mic, display |
| Waveshare ESP32-P4-NANO | Supported | Compact, onboard audio |
| Smart 86 Box 4" Panel | Supported | Touch display, wall-mount |
| Generic ESP32-P4 | Template | Customizable |

## Requirements

- **ESPHome** 2025.6.0以降（ESP32-P4サポート）
- **Home Assistant** 2023.5以降（Voice Assistant対応）
- **ESP-IDF Framework**（Arduinoは非対応）
- ESP32-P4開発ボード（マイク・スピーカー付き）

## Quick Start

### 1. Clone Repository

```bash
git clone https://github.com/98kuwa036/HomeAssistant-for-ESP32P4.git
cd HomeAssistant-for-ESP32P4
```

### 2. Configure Secrets

```bash
cp esphome/secrets.yaml.example esphome/secrets.yaml
```

`secrets.yaml`を編集して、WiFi認証情報とAPIキーを設定：

```yaml
wifi_ssid: "YourWiFiSSID"
wifi_password: "YourWiFiPassword"
ap_password: "FallbackPassword"
api_encryption_key: "YOUR_32_CHAR_KEY"  # esphome generate-encryption-key
ota_password: "YourOTAPassword"
```

### 3. Flash Device

```bash
# Install ESPHome
pip install esphome

# Flash (USB接続)
esphome run esphome/configs/esp32p4-voice-assistant.yaml
```

または、ESPHome Dashboardを使用：
1. Home Assistant → ESPHome Add-on
2. 設定ファイルをコピー
3. Install → Wirelessly or via USB

### 4. Add to Home Assistant

1. **Settings** → **Devices & Services**
2. 発見されたESPHomeデバイスを追加
3. APIキーを入力
4. Voice Assistantパイプラインを設定

## Project Structure

```
HomeAssistant-for-ESP32P4/
├── esphome/
│   ├── configs/
│   │   ├── esp32p4-voice-assistant.yaml  # Full featured config
│   │   ├── esp32p4-waveshare-nano.yaml   # Waveshare board
│   │   ├── esp32p4-smart86box.yaml       # Smart 86 Box panel
│   │   └── esp32p4-minimal.yaml          # Minimal template
│   ├── common/
│   │   ├── base.yaml                     # Base configuration
│   │   ├── wifi.yaml                     # WiFi settings
│   │   └── voice.yaml                    # Voice assistant config
│   └── secrets.yaml.example              # Secrets template
├── components/
│   ├── esp32_p4_audio/                   # P4 audio optimization
│   │   ├── __init__.py
│   │   ├── esp32_p4_audio.h
│   │   └── esp32_p4_audio.cpp
│   └── es8311/                           # ES8311 codec driver
│       ├── __init__.py
│       ├── es8311.h
│       └── es8311.cpp
├── docs/
│   ├── home-assistant-setup.md           # HA setup guide
│   ├── hardware-guide.md                 # Hardware documentation
│   └── CHANGELOG.md                      # Version history
├── LICENSE
└── README.md
```

## Configuration

### Basic Configuration

最小限の設定例：

```yaml
substitutions:
  device_name: my-voice-assistant
  i2s_bclk_pin: GPIO12
  i2s_ws_pin: GPIO13
  i2s_din_pin: GPIO14
  i2s_dout_pin: GPIO15

esphome:
  name: ${device_name}

esp32:
  board: esp32-p4-generic
  variant: esp32p4
  framework:
    type: esp-idf
    version: "5.3.1"

# ... (see esp32p4-minimal.yaml for complete example)
```

### Wake Word Configuration

ウェイクワードの変更：

```yaml
micro_wake_word:
  models:
    - model: hey_jarvis  # Options: okay_nabu, hey_jarvis, alexa, etc.
```

### LED Status Indicators

| Color | Meaning |
|-------|---------|
| Blue (pulse) | Listening |
| Cyan | Speech detected |
| Yellow | Processing |
| Purple | Speaking |
| Red (fast pulse) | Error |
| Green (flash) | Connected |

## Documentation

- [Home Assistant Setup Guide](docs/home-assistant-setup.md) - 詳細なセットアップ手順
- [Hardware Guide](docs/hardware-guide.md) - ピン配置とハードウェア情報
- [Changelog](docs/CHANGELOG.md) - 変更履歴

## Known Limitations

- **ESP-IDF Only**: Arduino frameworkは非対応
- **External Bluetooth**: ESP32-P4にはBluetooth非搭載、外部チップ（ESP32-C6）が必要
- **Beta Status**: 一部のESPHomeコンポーネントは開発中

## Troubleshooting

### デバイスが応答しない

1. シリアルログを確認: `esphome logs esp32p4-voice-assistant.yaml`
2. WiFi接続状態を確認
3. Home Assistant APIの接続状態を確認

### 音声が認識されない

1. マイクのピン設定を確認
2. ノイズ環境を改善
3. `noise_suppression_level`を調整

### 音声出力がない

1. スピーカーのピン設定を確認
2. `volume_multiplier`を増加
3. I2Sの設定を確認

## Contributing

Issues and Pull Requests are welcome!

1. Fork the repository
2. Create your feature branch
3. Commit your changes
4. Push to the branch
5. Create a Pull Request

## License

This project is licensed under the Apache License 2.0 - see the [LICENSE](LICENSE) file for details.

## References

- [ESPHome Voice Assistant](https://esphome.io/components/voice_assistant.html)
- [ESPHome 2025.6.0 Changelog](https://esphome.io/changelog/2025.6.0/) - ESP32-P4 support
- [ESPHome 2025.11.0 Changelog](https://esphome.io/changelog/2025.11.0/) - BLE via ESP-Hosted
- [Home Assistant Voice Control](https://www.home-assistant.io/voice_control/)
- [ESP32-P4 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-p4_datasheet_en.pdf)

## Acknowledgments

- [ESPHome Project](https://esphome.io/)
- [Home Assistant](https://www.home-assistant.io/)
- [Espressif Systems](https://www.espressif.com/)
