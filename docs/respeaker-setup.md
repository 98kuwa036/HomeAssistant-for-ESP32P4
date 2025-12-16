# ReSpeaker USB Mic Array セットアップガイド

ReSpeaker USB Mic Array を Raspberry Pi 4B に接続し、Home Assistant Assist で使用する手順です。

## システム構成

```
┌─────────────────────────────────────────────────────────────┐
│                    Raspberry Pi 4B                          │
│  ┌─────────────────┐    ┌─────────────────────────────────┐ │
│  │ Home Assistant  │    │ ReSpeaker USB Mic Array         │ │
│  │ - Assist        │◄───│ - 4ch マイクアレイ               │ │
│  │ - ESPHome       │    │ - ビームフォーミング              │ │
│  │ - Google Cloud  │    │ - ノイズキャンセル               │ │
│  └────────┬────────┘    └─────────────────────────────────┘ │
│           │ WiFi                                            │
└───────────┼─────────────────────────────────────────────────┘
            │
            ▼
┌─────────────────────────────────────────────────────────────┐
│              ESP32-P4-Function-EV-Board                     │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────────┐  │
│  │ ES8311   │  │ SCD40    │  │ SPS30    │  │ OV5640      │  │
│  │ DAC      │  │ CO2      │  │ PM       │  │ Camera      │  │
│  └────┬─────┘  └──────────┘  └──────────┘  └─────────────┘  │
│       │                                                      │
│       ▼                                                      │
│  ┌──────────┐                                               │
│  │ PAM8403  │──► Peerless Speakers × 2                      │
│  │ Amp      │                                               │
│  └──────────┘                                               │
└─────────────────────────────────────────────────────────────┘
```

## 1. ReSpeaker をRaspberry Pi に接続

1. ReSpeaker USB Mic Array を Raspberry Pi 4B の USB ポートに接続
2. SSH でRaspberry Pi にログイン

```bash
# Home Assistant OS の場合
ssh root@homeassistant.local -p 22222
```

3. デバイスが認識されているか確認

```bash
# Terminal & SSH アドオンから
ha host hardware info | grep -A5 "seeed"
```

## 2. Wyoming Protocol アドオンのインストール

ReSpeaker を Home Assistant Assist で使うには Wyoming Satellite を使用します。

### 2.1 Wyoming Satellite アドオン

```
Settings → Add-ons → Add-on Store →
  右上の「...」→ Repositories →
  追加: https://github.com/rhasspy/hassio-addons
```

**Wyoming Satellite** をインストール。

### 2.2 Wyoming Satellite 設定

Configuration タブ:

```yaml
microphone_device: "plughw:CARD=ArrayUAC10,DEV=0"
sound_device: ""  # スピーカーはESP32-P4から出力
wake_word: "ok_nabu"
```

### 2.3 オーディオデバイスの確認

```bash
# Terminal & SSH から
arecord -L | grep -i array
arecord -L | grep -i seeed
```

出力例:
```
plughw:CARD=ArrayUAC10,DEV=0
    seeed-voicecard, USB Audio
```

## 3. Assist Pipeline の設定

### 3.1 音声アシスタント設定

```
Settings → Voice assistants → Add Assistant
```

| 項目 | 設定 |
|------|------|
| Name | Smart Speaker |
| Language | Japanese |
| Conversation agent | Google Generative AI / OpenAI |
| Speech-to-text | Google Cloud STT |
| Text-to-speech | Google Cloud TTS |
| Wake word | ok_nabu |

### 3.2 Wyoming 統合の追加

```
Settings → Devices & Services → Add Integration → Wyoming Protocol
```

Host: `localhost` (または Wyoming Satellite のIPアドレス)
Port: `10400`

## 4. Google Cloud STT/TTS の設定

### 4.1 Google Cloud Console

1. [Google Cloud Console](https://console.cloud.google.com/) にアクセス
2. 新しいプロジェクトを作成
3. APIを有効化:
   - Cloud Speech-to-Text API
   - Cloud Text-to-Speech API
4. サービスアカウントを作成し、JSONキーをダウンロード

### 4.2 Home Assistant 設定

`configuration.yaml` に追加:

```yaml
# Google Cloud STT
stt:
  - platform: google_cloud
    key_file: /config/google-cloud-key.json
    language: ja-JP
    model: latest_long

# Google Cloud TTS
tts:
  - platform: google_cloud
    key_file: /config/google-cloud-key.json
    language: ja-JP
    voice: ja-JP-Neural2-B
    encoding: mp3
    speed: 1.0
    pitch: 0.0
```

### 4.3 Google AI Studio (LLM)

1. [Google AI Studio](https://aistudio.google.com/) でAPIキーを取得
2. Home Assistant に追加:

```
Settings → Devices & Services → Add Integration → Google Generative AI
```

API Key を入力。

## 5. ESP32-P4 スピーカー出力の設定

TTS音声は ESP32-P4 経由で PAM8403 → Peerless スピーカーから出力されます。

### 5.1 Media Player の確認

ESP32-P4 が Home Assistant に接続されると、メディアプレーヤーとして認識されます:

```
Settings → Devices & Services → ESPHome → Smart Speaker
```

「Smart Speaker Speaker」というメディアプレーヤーエンティティが表示されます。

### 5.2 Assist Pipeline でスピーカーを設定

Assist Pipeline 設定で、TTS出力先として ESP32-P4 のメディアプレーヤーを選択:

1. Assist Pipeline 設定を開く
2. Output device: `media_player.smart_speaker_speaker`

## 6. 動作確認

### 6.1 マイクテスト

```bash
# Terminal から
arecord -D plughw:CARD=ArrayUAC10,DEV=0 -f S16_LE -r 16000 -c 1 -d 5 test.wav
aplay test.wav
```

### 6.2 Assist 動作確認

1. 「OK Nabu」とウェイクワードを発声
2. 質問を話す（例：「今日の天気は？」）
3. ESP32-P4 スピーカーから応答が再生される

## トラブルシューティング

### ReSpeaker が認識されない

```bash
lsusb | grep -i seeed
dmesg | tail -20
```

### 音声が認識されない

1. マイクゲインを確認:
```bash
alsamixer -c ArrayUAC10
```

2. ノイズ環境を確認（静かな場所でテスト）

### スピーカーから音が出ない

1. ESP32-P4 が Home Assistant に接続されているか確認
2. Media Player エンティティが利用可能か確認
3. ES8311 の I2C 通信を確認

## ハードウェア接続図

### PAM8403 接続

```
ES8311 DAC Output ──┬── L+ ──► PAM8403 IN-L
                    │
                    └── R+ ──► PAM8403 IN-R

PAM8403 OUT-L ──► Peerless Speaker 1
PAM8403 OUT-R ──► Peerless Speaker 2

電源: 5V (USBまたは別電源)
```

### I2C センサー接続

```
ESP32-P4 GPIO7 (SDA) ──┬── SCD40 SDA
                       └── SPS30 SDA

ESP32-P4 GPIO8 (SCL) ──┬── SCD40 SCL
                       └── SPS30 SCL

電源: 3.3V (SCD40), 5V (SPS30)
```
