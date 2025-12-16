# ReSpeaker USB Mic Array セットアップガイド

ReSpeaker USB Mic Array を ESP32-P4 に直接接続して使用する手順です。

## システム構成

```
┌──────────────────────────────────────────────────────────────────────┐
│                   ESP32-P4-Function-EV-Board                         │
│                                                                      │
│  ┌─────────────────────┐    ┌────────────────────────────────────┐   │
│  │ USB Host Port       │◄───│ ReSpeaker USB Mic Array            │   │
│  │ (USB-A connector)   │    │ - 4ch マイクアレイ                  │   │
│  └─────────────────────┘    │ - ビームフォーミング                │   │
│                              │ - エコーキャンセル                  │   │
│  ┌─────────────────────┐    └────────────────────────────────────┘   │
│  │ ES8311 DAC          │                                             │
│  │ I2S Audio Output    │──► PAM8403 Amp ──► Peerless Speakers × 2   │
│  └─────────────────────┘                                             │
│                                                                      │
│  ┌─────────────────────┐    ┌─────────────────────┐                  │
│  │ SCD40 (I2C 0x62)    │    │ SPS30 (I2C 0x69)    │                  │
│  │ CO2/Temp/Humidity   │    │ PM1.0/2.5/4.0/10    │                  │
│  └─────────────────────┘    └─────────────────────┘                  │
│                                                                      │
│  ┌─────────────────────┐                                             │
│  │ OV5640 Camera       │ (MIPI CSI - 将来対応)                       │
│  └─────────────────────┘                                             │
│                                                                      │
│  ┌─────────────────────┐                                             │
│  │ ESP32-C6 WiFi 6     │──► Raspberry Pi 4B (Home Assistant)        │
│  └─────────────────────┘                                             │
└──────────────────────────────────────────────────────────────────────┘
```

## 必要なハードウェア

| コンポーネント | 型番 | 説明 |
|--------------|------|------|
| 開発ボード | ESP32-P4-Function-EV-Board v1.5 | USB Host対応 |
| マイクアレイ | ReSpeaker USB Mic Array | 4ch, ビームフォーミング |
| アンプ | PAM8403 | 3W × 2ch |
| スピーカー | Peerless × 2 | フルレンジ |
| CO2センサー | SCD40 | I2C接続 |
| PMセンサー | SPS30 | I2C接続 |
| カメラ (オプション) | OV5640 | MIPI CSI (将来対応) |

## 1. ハードウェア接続

### 1.1 ReSpeaker USB Mic Array

ESP32-P4-Function-EV-Board の **USB-A Host ポート**に接続します。

```
ESP32-P4 USB-A Host ◄── ReSpeaker USB Mic Array
```

> **注意**: USB-C ポートではなく、USB-A ポートに接続してください。

### 1.2 PAM8403 アンプ接続

ES8311 の DAC 出力を PAM8403 に接続します。

```
ESP32-P4 EV-Board Audio Header
    │
    ├── L_OUT (左チャンネル) ──► PAM8403 L-IN
    ├── R_OUT (右チャンネル) ──► PAM8403 R-IN
    └── GND ──────────────────► PAM8403 GND

PAM8403
    ├── L-OUT+ / L-OUT- ──► Peerless Speaker 1
    ├── R-OUT+ / R-OUT- ──► Peerless Speaker 2
    └── VCC ──────────────► 5V 電源
```

### 1.3 I2C センサー接続

```
ESP32-P4 GPIO7 (SDA) ──┬── SCD40 SDA ── SPS30 SDA
                       │
ESP32-P4 GPIO8 (SCL) ──┼── SCD40 SCL ── SPS30 SCL
                       │
3.3V ──────────────────┼── SCD40 VCC
                       │
5V ────────────────────┴── SPS30 VCC
GND ───────────────────── 全デバイス GND
```

## 2. ソフトウェアセットアップ

### 2.1 Home Assistant OS インストール

1. [Raspberry Pi Imager](https://www.raspberrypi.com/software/) をダウンロード
2. OS 選択: **Home Assistant OS (64-bit)**
3. SDカードに書き込み
4. Raspberry Pi 4B に挿入して起動

### 2.2 ESPHome アドオン

```
Settings → Add-ons → Add-on Store → ESPHome → Install
```

### 2.3 secrets.yaml 設定

ESPHome ダッシュボードで **SECRETS** をクリック:

```yaml
wifi_ssid: "YourWiFiSSID"
wifi_password: "YourWiFiPassword"
ap_password: "FallbackPassword123"
api_encryption_key: "生成したキーを貼り付け"
ota_password: "OTAPassword123"
```

暗号化キーの生成:
```bash
# Terminal & SSH アドオンから
docker exec -it addon_esphome esphome generate-encryption-key
```

### 2.4 ESP32-P4 設定ファイル

ESPHome ダッシュボードで:

1. **NEW DEVICE** → **SKIP**
2. 名前: `esp32p4-smart-speaker`
3. `esp32p4-respeaker-smart-speaker.yaml` の内容をコピー&ペースト
4. **SAVE** → **INSTALL**

### 2.5 初回フラッシュ

**INSTALL** → **Plug into this computer**

ESP32-P4 を USB-C ケーブルでPCに接続し、シリアルポートを選択してフラッシュします。

## 3. Home Assistant 設定

### 3.1 デバイス追加

ESP32-P4 が起動すると、Home Assistant が自動検出します。

```
Settings → Devices & Services → ESPHome → Configure
```

`api_encryption_key` を入力して接続。

### 3.2 Assist Pipeline 設定

```
Settings → Voice assistants → Add Assistant
```

| 項目 | 設定 |
|------|------|
| Name | Smart Speaker |
| Language | Japanese |
| Conversation agent | Google Generative AI |
| Speech-to-text | Google Cloud STT |
| Text-to-speech | Google Cloud TTS |
| Wake word | ok_nabu |

### 3.3 Google Cloud API 設定

詳細は [raspberry-pi-setup.md](raspberry-pi-setup.md) を参照してください。

## 4. 動作確認

### 4.1 ウェイクワードテスト

「OK Nabu」と発声すると:
1. LED が青く点滅（リスニング）
2. 質問を話す
3. LED が黄色（処理中）
4. スピーカーから応答

### 4.2 ボタン操作

Boot ボタン（GPIO0）を押すと:
- 待機中 → 音声アシスタント起動
- 動作中 → 音声アシスタント停止

### 4.3 センサー確認

Home Assistant のダッシュボードで以下を確認:
- CO2 (ppm)
- Temperature (°C)
- Humidity (%)
- PM2.5 (µg/m³)
- AQI (0-500)

## 5. トラブルシューティング

### ReSpeaker が認識されない

1. USB-A ポートに接続しているか確認（USB-C ではない）
2. ESPHome ログを確認:
   ```
   esphome logs esp32p4-smart-speaker.yaml
   ```
3. USB Host が有効になっているか確認

### 音声が認識されない

1. ReSpeaker の LED が光っているか確認
2. `noise_suppression_level` を調整（0-4）
3. `auto_gain` を調整（0-31dBFS）

### スピーカーから音が出ない

1. PAM8403 の電源（5V）を確認
2. ES8311 の I2C 通信を確認
3. `volume_multiplier` を増加（1.0-3.0）

### センサーが読み取れない

1. I2C 接続を確認
2. センサーのアドレスを確認:
   - SCD40: 0x62
   - SPS30: 0x69
3. I2C 周波数が 100kHz になっているか確認

## 6. LED 状態一覧

| 色/パターン | 状態 |
|------------|------|
| 緑点灯 | Home Assistant 接続済み |
| オレンジ点滅 | 接続切断 |
| 青点滅（遅い） | 待機中 |
| 青点滅（速い） | リスニング中 |
| シアン点滅 | 音声検出中 |
| 黄点滅 | 処理中 |
| 紫点滅 | 応答再生中 |
| 赤点滅 | エラー |

## 7. 注意事項

### USB Host の制限

- ESP32-P4 の USB Host は USB 2.0 Full Speed (12 Mbps) をサポート
- ReSpeaker USB Mic Array は 16kHz/16bit で十分な帯域

### カメラ (OV5640)

- ESP32-P4 は MIPI CSI をハードウェアでサポート
- ESPHome のネイティブ対応は開発中
- 対応後、設定ファイルを更新予定

### 電源要件

- ESP32-P4: 5V/2A 以上推奨
- ReSpeaker USB: ESP32-P4 から給電
- PAM8403: 別途 5V 電源推奨（大音量時）
