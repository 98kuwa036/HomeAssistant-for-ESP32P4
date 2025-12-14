# Raspberry Pi 4B + Google AI Studio 構成ガイド

このガイドでは、Raspberry Pi 4BでHome Assistantを運用し、Google AI Studio API (Gemma 3) を使用した音声アシスタント構成を説明します。

## 推奨構成

```
┌─────────────────┐     WiFi      ┌─────────────────────┐
│   ESP32-P4      │◄────────────►│  Raspberry Pi 4B    │
│  Voice Satellite│               │  Home Assistant     │
│  - マイク       │               │  - Assist Pipeline  │
│  - スピーカー   │               │  - 統合管理         │
└─────────────────┘               └──────────┬──────────┘
                                             │ Internet
                                             ▼
                          ┌──────────────────────────────────┐
                          │        Cloud Services            │
                          │  - Google AI Studio (Gemma 3)    │
                          │  - Google Cloud STT              │
                          │  - Google Cloud TTS              │
                          └──────────────────────────────────┘
```

## なぜクラウドオフロードが必要か

Raspberry Pi 4Bは優れたシングルボードコンピューターですが、音声処理には制限があります：

| 処理 | ローカル (Pi 4B) | クラウド |
|------|-----------------|---------|
| STT (音声→テキスト) | 遅い、精度低 | 高速、高精度 |
| LLM (会話処理) | メモリ不足 | Gemma 3対応 |
| TTS (テキスト→音声) | 遅い | 高速、自然 |

**推奨**: STT、LLM、TTSすべてをクラウドにオフロード

## セットアップ手順

### 1. Google AI Studio APIキーの取得

1. [Google AI Studio](https://aistudio.google.com/) にアクセス
2. Googleアカウントでログイン
3. **Get API Key** → **Create API Key** をクリック
4. APIキーをコピーして安全に保管

### 2. Google Cloud Platform設定（STT/TTS用）

#### プロジェクト作成
1. [Google Cloud Console](https://console.cloud.google.com/) にアクセス
2. 新しいプロジェクトを作成
3. 課金を有効化（無料枠あり）

#### APIの有効化
1. **APIs & Services** → **Enable APIs**
2. 以下のAPIを有効化：
   - Cloud Speech-to-Text API
   - Cloud Text-to-Speech API

#### サービスアカウント作成
1. **IAM & Admin** → **Service Accounts**
2. **Create Service Account**
3. 役割: **Cloud Speech Client** と **Cloud Text-to-Speech Client**
4. JSONキーをダウンロード

### 3. Home Assistant統合のインストール

#### Google Generative AI (Gemma 3用)

1. **Settings** → **Devices & Services** → **Add Integration**
2. **Google Generative AI** を検索して追加
3. APIキーを入力

#### Google Cloud STT

`configuration.yaml` に追加：

```yaml
# Google Cloud Speech-to-Text
stt:
  - platform: google_cloud
    key_file: /config/google_cloud_key.json
    language: ja-JP
    model: latest_long
```

#### Google Cloud TTS

`configuration.yaml` に追加：

```yaml
# Google Cloud Text-to-Speech
tts:
  - platform: google_cloud
    key_file: /config/google_cloud_key.json
    language: ja-JP
    gender: female
    voice: ja-JP-Neural2-B
    encoding: mp3
    speed: 1.0
    pitch: 0.0
```

### 4. Assist Pipeline設定

1. **Settings** → **Voice assistants** → **Add assistant**

2. 設定内容：

| 項目 | 設定値 |
|------|--------|
| Name | ESP32-P4 Voice Assistant |
| Language | Japanese (日本語) |
| Conversation agent | Google Generative AI |
| Speech-to-text | google_cloud |
| Text-to-speech | google_cloud |

3. **Google Generative AI** の会話エージェント設定：
   - Model: `gemini-1.5-flash` または `gemma-3`
   - Temperature: 0.7
   - Max tokens: 150

### 5. ESP32-P4デバイスの割り当て

1. **Settings** → **Devices & Services** → **ESPHome**
2. ESP32-P4デバイスを選択
3. **Configure** → 作成したパイプラインを選択

## Raspberry Pi 4B最適化

### システム設定

`/boot/config.txt` に追加（オプション）：

```ini
# GPU メモリを削減してRAMを確保
gpu_mem=64

# オーバークロック（冷却必須）
# over_voltage=2
# arm_freq=1800
```

### Home Assistant設定

`configuration.yaml` に追加：

```yaml
# レコーダー最適化（SDカード書き込み削減）
recorder:
  purge_keep_days: 5
  commit_interval: 30
  exclude:
    domains:
      - automation
      - script
    entity_globs:
      - sensor.esp32p4_*_wifi_signal

# ロガー最適化
logger:
  default: warning
  logs:
    homeassistant.components.google_generative_ai_conversation: info
    homeassistant.components.stt: info
    homeassistant.components.tts: info
```

### Swapの拡張

```bash
# Swapを2GBに拡張
sudo dphys-swapfile swapoff
sudo nano /etc/dphys-swapfile
# CONF_SWAPSIZE=2048 に変更
sudo dphys-swapfile setup
sudo dphys-swapfile swapon
```

## 日本語音声設定

### 推奨TTS Voice

| Voice ID | 特徴 |
|----------|------|
| ja-JP-Neural2-B | 女性、自然 |
| ja-JP-Neural2-C | 男性、自然 |
| ja-JP-Neural2-D | 男性、落ち着いた |
| ja-JP-Wavenet-A | 女性、標準 |

### カスタムプロンプト（Google AI設定）

Home Assistantの会話エージェント設定で、以下のシステムプロンプトを設定：

```
あなたはスマートホームアシスタントです。
簡潔で自然な日本語で応答してください。
応答は50文字以内にしてください。
家電の操作を依頼された場合は、確認してから実行してください。
```

## 料金目安

### Google AI Studio (Gemma 3)
- 無料枠: 60リクエスト/分
- 個人使用では通常無料

### Google Cloud STT
- 無料枠: 60分/月
- 超過: $0.006/15秒

### Google Cloud TTS
- 無料枠: 400万文字/月 (Neural2)
- 超過: $0.000016/文字

**一般的な家庭使用**: 月額 $0〜5程度

## トラブルシューティング

### 応答が遅い

1. WiFi信号強度を確認（ESP32-P4のセンサー値）
2. Raspberry PiのCPU/メモリ使用率を確認
3. Google Cloudのリージョンを確認（asia-northeast1推奨）

### 日本語が認識されない

1. STTの言語設定が `ja-JP` か確認
2. マイクのゲインを調整（ESPHome設定）
3. 周囲のノイズを軽減

### LLMの応答が不適切

1. システムプロンプトを調整
2. Temperature値を下げる（0.3〜0.5）
3. Home Assistantのエンティティ情報を確認

### 音声出力の品質

1. TTS voiceをNeural2系に変更
2. ESP32-P4のスピーカー音量を調整
3. ビットレートを確認

## 参考リンク

- [Google AI Studio](https://aistudio.google.com/)
- [Google Cloud Speech-to-Text](https://cloud.google.com/speech-to-text)
- [Google Cloud Text-to-Speech](https://cloud.google.com/text-to-speech)
- [Home Assistant Voice Assistant](https://www.home-assistant.io/voice_control/)
- [Google Generative AI Integration](https://www.home-assistant.io/integrations/google_generative_ai_conversation/)
