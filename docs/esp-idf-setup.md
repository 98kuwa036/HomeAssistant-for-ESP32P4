# ESP-IDF 開発環境セットアップガイド

ReSpeaker USB Mic Array を ESP32-P4 で使用するための ESP-IDF 開発環境をセットアップします。

## なぜ ESP-IDF を使うのか？

ESPHome は便利ですが、以下の理由で ESP-IDF を使用します：

1. **USB Audio Class (UAC)** - ESPHome はまだ ESP32-P4 の USB Host UAC を完全サポートしていない
2. **MIPI CSI カメラ** - OV5640 カメラサポートも ESP-IDF が先行
3. **最大限のパフォーマンス** - C言語で直接制御することで最適化可能

## システム構成

```
┌──────────────────────────────────────────────────────────────────┐
│                  ESP32-P4-Function-EV-Board                      │
│                                                                  │
│  ┌─────────────────┐    ┌──────────────────────────────────┐     │
│  │ USB-A Host      │◄───│ ReSpeaker USB Mic Array          │     │
│  │ (USB Host UAC)  │    │ (XVF3800)                        │     │
│  └─────────────────┘    └──────────────────────────────────┘     │
│                                                                  │
│  ┌─────────────────┐    ┌──────────────────────────────────┐     │
│  │ ES8311 DAC      │──► │ PAM8403 Amp → Peerless × 2       │     │
│  │ (I2S Output)    │    └──────────────────────────────────┘     │
│  └─────────────────┘                                             │
│                                                                  │
│  ┌─────────────────┐    ┌─────────────────┐                      │
│  │ SCD40 (I2C)     │    │ SPS30 (I2C)     │                      │
│  │ CO2/Temp/Hum    │    │ PM Sensors      │                      │
│  └─────────────────┘    └─────────────────┘                      │
│                                                                  │
│  ┌─────────────────┐                                             │
│  │ ESP32-C6        │ ──► WiFi ──► Raspberry Pi 4B               │
│  │ (WiFi 6)        │              (Home Assistant)               │
│  └─────────────────┘                                             │
└──────────────────────────────────────────────────────────────────┘
```

## 1. 開発環境のインストール

### 1.1 VS Code + ESP-IDF Extension

1. [VS Code](https://code.visualstudio.com/) をインストール
2. Extensions から **ESP-IDF** を検索してインストール
3. コマンドパレット (`Ctrl+Shift+P`) → **ESP-IDF: Configure ESP-IDF Extension**
4. **EXPRESS** を選択
5. ESP-IDF バージョン: **v5.3.x** 以上を選択
6. インストール完了まで待機（約10-20分）

### 1.2 ESP-ADF のセットアップ（オプション）

USB Audio の高度な機能を使う場合：

```bash
# ESP-ADF をクローン
cd ~/esp
git clone --recursive https://github.com/espressif/esp-adf.git

# 環境変数を設定
echo 'export ADF_PATH=~/esp/esp-adf' >> ~/.bashrc
source ~/.bashrc
```

## 2. プロジェクトのビルド

### 2.1 プロジェクトを開く

```bash
cd HomeAssistant-for-ESP32P4/esp-idf
```

VS Code でフォルダを開く。

### 2.2 ターゲットを設定

```bash
idf.py set-target esp32p4
```

または VS Code の下部ステータスバーで **ESP32-P4** を選択。

### 2.3 設定

```bash
idf.py menuconfig
```

**ESP32-P4 Smart Speaker Configuration** メニューで設定：

| 項目 | 説明 | 例 |
|------|------|-----|
| WiFi SSID | WiFiネットワーク名 | `MyHomeWiFi` |
| WiFi Password | WiFiパスワード | `mypassword123` |
| HA Host | Home Assistantホスト | `192.168.1.100` |
| HA Port | Home Assistantポート | `8123` |
| HA Token | 長期アクセストークン | `eyJhbGc...` |

### 2.4 Home Assistant トークンの取得

1. Home Assistant にログイン
2. 左下のプロフィールアイコンをクリック
3. **長期間アクセストークン** → **トークンを作成**
4. 名前: `ESP32-P4 Smart Speaker`
5. 生成されたトークンをコピー

### 2.5 ビルド

```bash
idf.py build
```

### 2.6 フラッシュ

ESP32-P4 を USB-C で接続し：

```bash
idf.py -p /dev/ttyUSB0 flash
```

Windows の場合：
```bash
idf.py -p COM3 flash
```

### 2.7 モニター

```bash
idf.py -p /dev/ttyUSB0 monitor
```

## 3. ハードウェア接続

### 3.1 ReSpeaker USB Mic Array

ESP32-P4-Function-EV-Board の **USB-A ポート**に接続。

### 3.2 PAM8403 アンプ

ES8311 オーディオ出力ヘッダから：

```
Audio Header    PAM8403
L_OUT      →    L-IN
R_OUT      →    R-IN
GND        →    GND

VCC (5V)   →    5V電源（別途供給推奨）
```

### 3.3 I2C センサー

```
GPIO7 (SDA) → SCD40 SDA + SPS30 SDA
GPIO8 (SCL) → SCD40 SCL + SPS30 SCL
3.3V        → SCD40 VCC
5V          → SPS30 VCC
GND         → 共通GND
```

## 4. 動作確認

### 4.1 シリアルログ

正常起動時のログ：

```
I (0) MAIN: ESP32-P4 Smart Speaker Starting...
I (100) MAIN: Hardware: ReSpeaker USB + PAM8403 + SCD40 + SPS30
I (200) MAIN: GPIO initialized
I (300) WIFI_MGR: Initializing WiFi manager
I (400) WIFI_MGR: WiFi started, connecting...
I (1500) WIFI_MGR: Got IP: 192.168.1.50
I (1600) USB_AUDIO: Initializing USB Audio:
I (1600) USB_AUDIO:   Sample rate: 16000 Hz
I (1600) USB_AUDIO:   Channels: 1
I (1700) USB_AUDIO: USB Host installed, waiting for devices...
I (2000) USB_AUDIO: ReSpeaker USB Mic Array connected
I (2100) I2S_OUTPUT: I2S output initialized successfully
I (2200) HA_CLIENT: Connected to Home Assistant
I (2300) MAIN: All tasks started. System ready.
```

### 4.2 トラブルシューティング

#### USB デバイスが認識されない

1. USB-A ポートに接続しているか確認
2. シリアルログで USB Host エラーを確認
3. `menuconfig` で USB Host が有効か確認

#### WiFi 接続失敗

1. SSID/パスワードを確認
2. ESP32-C6 が正しく動作しているか確認
3. ルーターとの距離を確認

#### Home Assistant 接続失敗

1. トークンが正しいか確認
2. HA ホスト/ポートを確認
3. HA API が有効か確認（設定 → API）

## 5. 次のステップ

1. **USB Audio 完全実装** - ESP-ADF の UAC ドライバを統合
2. **Wake Word 検出** - Sensory または Picovoice を統合
3. **Voice Pipeline** - Home Assistant Assist と完全連携
4. **MIPI カメラ** - OV5640 を使った顔認識ウェイク

## 6. 参考リンク

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/)
- [ESP-ADF Documentation](https://docs.espressif.com/projects/esp-adf/en/latest/)
- [ESP32-P4 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp32-p4_technical_reference_manual_en.pdf)
- [Home Assistant REST API](https://developers.home-assistant.io/docs/api/rest/)
