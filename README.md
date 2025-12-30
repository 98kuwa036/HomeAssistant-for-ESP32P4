# Omni-P4: Home Assistant Voice Assistant for ESP32-P4

ESP32-P4-Function-EV-Board を使用した Home Assistant 音声アシスタント実装です。
HP ProDesk 600 G4 + Proxmox VE + ローカルLLM (Qwen2.5 3B) 構成に最適化されています。

> **Beta Version**: ESP32-P4のESPHomeサポートは2025年6月から開始されており、一部の機能はまだ開発中です。

## Target Hardware

### ESP32-P4 端末 (Voice Satellite)

| コンポーネント | 詳細 |
|--------------|------|
| **開発ボード** | [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/) v1.5 |
| **CPU** | ESP32-P4 Dual-core RISC-V @ 400MHz |
| **マイク** | ReSpeaker USB Mic Array v2.0 (XVF3800, USB Audio Class) |
| **DAC** | ES9038Q2M (I2S, 32bit/384kHz) |
| **スピーカー** | Peerless PLS-50N25AL02 |
| **WiFi/BLE** | ESP32-C6-MINI-1 (WiFi 6) |
| **Memory** | 32MB PSRAM, 16MB Flash |

### サーバー (Home Assistant + LLM)

| コンポーネント | 詳細 |
|--------------|------|
| **ハードウェア** | HP ProDesk 600 G4 Mini |
| **CPU** | Intel Core i5-8500 (6C/6T, 3.0-4.1GHz) |
| **メモリ** | 16GB DDR4 (8GB x 2) |
| **仮想化** | Proxmox VE 8.x |
| **コンテナ1** | HAOS (Home Assistant OS) |
| **コンテナ2** | Ubuntu Server 24.04 (Ollama + Qwen2.5) |

## Features

- **Voice Control**: Home Assistantと連携した音声アシスタント
- **Wake Word**: オンデバイス検出（"Okay Nabu"）
- **ES8311 Codec**: 高品質オーディオ入出力
- **Cloud Offload**: STT/TTS/LLMをGoogle Cloudで処理
- **Japanese Support**: 日本語音声対応
- **Environmental Sensors** (Smart Speaker版):
  - SCD40: CO2、温度、湿度
  - SPS30: PM1.0, PM2.5, PM4.0, PM10
  - Air Quality Index 計算

## System Architecture

```
┌───────────────────────────────────────────────────────────────┐
│              ESP32-P4-Function-EV-Board (Voice Satellite)     │
│  ┌────────────────┐  ┌────────────┐  ┌──────────────────────┐ │
│  │ ReSpeaker USB  │  │ ES9038Q2M  │  │ ESP32-C6-MINI-1      │ │
│  │ XVF3800        │  │ Hi-Res DAC │  │ WiFi 6 + BLE 5       │ │
│  │ Beamforming    │  │ 32bit/384k │  │                      │ │
│  └───────┬────────┘  └─────┬──────┘  └──────────┬───────────┘ │
│          │ USB Host        │ I2S               │              │
│          └────────┬────────┘                   │              │
│                   │                            │              │
│  ┌────────────────▼────────────────────────────▼────────────┐ │
│  │           Audio Pipeline (Dual Buffer)                    │ │
│  │   RAW: 48kHz Stereo ──────► Local LLM (高音質)           │ │
│  │   Processed: 16kHz Mono ──► ESPHome/HA                   │ │
│  └──────────────────────────────────────────────────────────┘ │
└───────────────────────────────┬───────────────────────────────┘
                                │ WiFi (Same LAN)
                                ▼
┌───────────────────────────────────────────────────────────────┐
│                   HP ProDesk 600 G4 Mini                      │
│                   Proxmox VE 8.x                              │
│  ┌─────────────────────────┐  ┌─────────────────────────────┐ │
│  │  VM: Home Assistant OS  │  │  LXC: Ubuntu Server 24.04   │ │
│  │  - ESPHome Integration  │  │  - Ollama (Qwen2.5 3B)      │ │
│  │  - Assist Pipeline      │  │  - Immich (Photo AI)        │ │
│  │  - Whisper (STT)        │  │  - Open WebUI               │ │
│  │  - Piper (TTS)          │  │  - rclone (Google Drive)    │ │
│  └─────────────────────────┘  └─────────────────────────────┘ │
└───────────────────────────────────────────────────────────────┘
                                │
                                ▼ (Optional: Nature Remo)
┌───────────────────────────────────────────────────────────────┐
│                      Nature Remo mini                         │
│                  (Local API for IR Control)                   │
└───────────────────────────────────────────────────────────────┘
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

### サーバー要件
- **HP ProDesk 600 G4** (または同等のx86-64マシン)
- **Proxmox VE** 8.x
- **Home Assistant OS** 2024.x以降
- **Ubuntu Server** 24.04 LTS

### ESP32-P4 要件
- **ESPHome** 2025.6.0以降
- **ESP-IDF** v5.3以降 (ReSpeaker USB構成)

### LLM/音声処理
- **Ollama** + **Qwen2.5 3B** (ローカルLLM)
- **Whisper** (ローカルSTT)
- **Piper** (ローカルTTS)

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

[HP ProDesk 600 G4 + Proxmox VE 構成ガイド](docs/proxmox-setup.md) を参照してください。

## Development Approaches

本プロジェクトは2つの開発アプローチを提供しています：

### ESPHome (推奨: 基本構成)

ES8311内蔵マイクを使用する基本構成向け。設定が簡単で Home Assistant との統合も容易。

```bash
esphome run esphome/configs/esp32p4-function-ev-board.yaml
```

### ESP-IDF (推奨: ReSpeaker USB 構成)

ReSpeaker USB Mic Array を使用する高機能構成向け。ESPHome の USB Audio Class サポートがまだ実験段階のため、C言語での直接開発が必要。

```bash
cd esp-idf
idf.py set-target esp32p4
idf.py menuconfig  # WiFi/HA設定
idf.py build flash
```

詳細は [ESP-IDF 開発環境セットアップガイド](docs/esp-idf-setup.md) を参照。

## Available Configurations

| 設定ファイル | 用途 | マイク | センサー |
|------------|------|--------|---------|
| `esp32p4-function-ev-board.yaml` | 基本音声アシスタント | ES8311内蔵 | なし |
| `esp32p4-smart-speaker.yaml` | 高機能スマートスピーカー | ES8311内蔵 | SCD40, SPS30 |
| `esp32p4-respeaker-smart-speaker.yaml` | ReSpeaker構成 | ReSpeaker USB (ESP32-P4) | SCD40, SPS30 |
| `esp32p4-lightweight.yaml` | 軽量版（RPi 4B最適化） | ES8311内蔵 | なし |

## Project Structure

```
HomeAssistant-for-ESP32P4/
├── esphome/                   # ESPHome 設定 (基本構成)
│   ├── configs/
│   │   ├── esp32p4-function-ev-board.yaml
│   │   ├── esp32p4-smart-speaker.yaml
│   │   └── esp32p4-lightweight.yaml
│   ├── common/
│   └── secrets.yaml.example
├── esp-idf/                   # ESP-IDF プロジェクト (ReSpeaker USB)
│   ├── main/
│   │   ├── main.c             # メインアプリケーション
│   │   ├── usb_audio.c        # USB Audio (ReSpeaker)
│   │   ├── i2s_output.c       # I2S 出力 (ES8311)
│   │   ├── wifi_manager.c     # WiFi 管理
│   │   └── ha_client.c        # Home Assistant 連携
│   ├── sdkconfig.defaults
│   └── CMakeLists.txt
├── components/                # ESPHome カスタムコンポーネント
├── docs/
│   ├── esp-idf-setup.md       # ESP-IDF 開発ガイド
│   ├── respeaker-setup.md     # ReSpeaker 構成ガイド
│   ├── raspberry-pi-setup.md
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

- **[HP ProDesk 600 G4 + Proxmox VE Setup](docs/proxmox-setup.md)** - サーバー構成ガイド
- **[ESP-IDF 開発環境セットアップ](docs/esp-idf-setup.md)** - ReSpeaker USB 構成向け
- **[ReSpeaker USB + ES9038Q2M Setup](docs/respeaker-setup.md)** - オーディオ構成ガイド
- [Hardware Guide](docs/hardware-guide.md) - ハードウェア詳細
- [Changelog](docs/CHANGELOG.md) - 変更履歴
- [Raspberry Pi Setup (Legacy)](docs/raspberry-pi-setup.md) - 旧RPi4B構成（参考用）

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
- [Proxmox VE Documentation](https://pve.proxmox.com/wiki/Main_Page)
- [Ollama Documentation](https://ollama.ai/docs)
- [Home Assistant Assist](https://www.home-assistant.io/voice_control/)
- [ReSpeaker USB Mic Array](https://wiki.seeedstudio.com/ReSpeaker_Mic_Array_v2.0/)

## License

Apache License 2.0 - see [LICENSE](LICENSE)
