# HP ProDesk 600 G4 + Proxmox VE 構成ガイド

このガイドでは、HP ProDesk 600 G4 Mini に Proxmox VE をインストールし、Home Assistant と ローカルLLM (Qwen2.5) を構成する方法を説明します。

## 対象ハードウェア

| コンポーネント | 詳細 |
|--------------|------|
| **サーバー** | HP ProDesk 600 G4 Mini |
| **CPU** | Intel Core i5-8500 (6C/6T, 3.0-4.1GHz) |
| **メモリ** | 16GB DDR4 (8GB x 2) |
| **ストレージ** | 256GB NVMe SSD (推奨: 512GB以上) |
| **Voice Satellite** | ESP32-P4-Function-EV-Board v1.5 |
| **LLM** | Ollama + Qwen2.5 3B (ローカル) |

## アーキテクチャ

```
┌──────────────────────────────────────────────────────────────┐
│                    HP ProDesk 600 G4 Mini                    │
│                       Proxmox VE 8.x                         │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────────────────────┐  ┌───────────────────────────┐  │
│  │  VM: Home Assistant OS  │  │  LXC: Ubuntu Server 24.04 │  │
│  │  ────────────────────── │  │  ───────────────────────  │  │
│  │  • ESPHome Integration  │  │  • Ollama (Qwen2.5 3B)    │  │
│  │  • Whisper (STT)        │  │  • Open WebUI             │  │
│  │  • Piper (TTS)          │  │  • Immich                 │  │
│  │  • Assist Pipeline      │  │  • rclone                 │  │
│  │  • Nature Remo          │  │                           │  │
│  │                         │  │                           │  │
│  │  RAM: 4GB               │  │  RAM: 8GB                 │  │
│  │  Disk: 32GB             │  │  Disk: 100GB+             │  │
│  └─────────────────────────┘  └───────────────────────────┘  │
│                                                              │
└──────────────────────────────────────────────────────────────┘
                              │
                              │ LAN (192.168.x.x)
                              ▼
         ┌────────────────────────────────────────┐
         │  ESP32-P4-Function-EV-Board            │
         │  • ReSpeaker USB Mic Array             │
         │  • ES9038Q2M DAC                       │
         │  • Peerless Speaker                    │
         └────────────────────────────────────────┘
```

## なぜローカルLLMか

Raspberry Pi 4B と比較した利点：

| 項目 | Raspberry Pi 4B | HP ProDesk 600 G4 |
|------|-----------------|-------------------|
| CPU | ARM Cortex-A72 4C | Intel i5-8500 6C |
| メモリ | 4-8GB | 16GB |
| LLM | 不可能 | Qwen2.5 3B 対応 |
| STT | クラウド依存 | Whisper ローカル |
| TTS | クラウド依存 | Piper ローカル |
| プライバシー | クラウド送信 | 完全ローカル |
| 月額コスト | $5-20 | $0 (電気代のみ) |
| 応答速度 | 1-3秒 | 0.5-1秒 |

## セットアップ手順

### 1. Proxmox VE インストール

#### 1.1 ISOダウンロード

```bash
# 公式サイトからダウンロード
# https://www.proxmox.com/en/downloads
```

#### 1.2 USBブート作成

```bash
# Linux/Mac
sudo dd if=proxmox-ve_8.x.iso of=/dev/sdX bs=4M status=progress

# Windows: Rufus または Etcher を使用
```

#### 1.3 インストール

1. USBから起動
2. 「Install Proxmox VE」を選択
3. ターゲットディスクを選択
4. 国・タイムゾーン・キーボードを設定
5. rootパスワードとメールを設定
6. ネットワーク設定：
   - Hostname: `proxmox.local`
   - IP: `192.168.1.100/24` (固定IP推奨)
   - Gateway: `192.168.1.1`
   - DNS: `192.168.1.1` または `8.8.8.8`

#### 1.4 初期設定

インストール後、ブラウザで `https://192.168.1.100:8006` にアクセス。

```bash
# SSH接続
ssh root@192.168.1.100

# リポジトリ設定（エンタープライズなし）
echo "deb http://download.proxmox.com/debian/pve bookworm pve-no-subscription" > /etc/apt/sources.list.d/pve-no-subscription.list

# エンタープライズリポジトリを無効化
mv /etc/apt/sources.list.d/pve-enterprise.list /etc/apt/sources.list.d/pve-enterprise.list.disabled

# アップデート
apt update && apt dist-upgrade -y
```

### 2. Home Assistant OS (HAOS) VM

#### 2.1 HAOS イメージダウンロード

```bash
# Proxmoxホストで実行
cd /var/lib/vz/template/iso/

# 最新バージョンを確認
# https://github.com/home-assistant/operating-system/releases
wget https://github.com/home-assistant/operating-system/releases/download/14.0/haos_ova-14.0.qcow2.xz

# 解凍
xz -d haos_ova-14.0.qcow2.xz
```

#### 2.2 VM作成

```bash
# VM作成 (ID: 100)
qm create 100 --name haos --memory 4096 --cores 2 --net0 virtio,bridge=vmbr0

# ディスクインポート
qm importdisk 100 /var/lib/vz/template/iso/haos_ova-14.0.qcow2 local-lvm

# ディスクをVMにアタッチ
qm set 100 --scsihw virtio-scsi-pci --scsi0 local-lvm:vm-100-disk-0

# ブート設定
qm set 100 --boot order=scsi0

# BIOS設定 (UEFI)
qm set 100 --bios ovmf --efidisk0 local-lvm:1,format=raw,efitype=4m,pre-enrolled-keys=1

# VM起動
qm start 100
```

#### 2.3 Home Assistant 初期設定

1. `http://192.168.1.xxx:8123` にアクセス（IPはDHCPで割り当て）
2. ユーザーアカウント作成
3. ロケーション・通貨設定

### 3. Ubuntu Server LXC コンテナ

#### 3.1 テンプレートダウンロード

Proxmox Web UI:
- Datacenter → Storage → local → CT Templates
- 「Templates」ボタン → `ubuntu-24.04-standard` をダウンロード

または CLI:

```bash
pveam update
pveam download local ubuntu-24.04-standard_24.04-2_amd64.tar.zst
```

#### 3.2 LXC作成

```bash
# LXC作成 (ID: 101)
pct create 101 local:vztmpl/ubuntu-24.04-standard_24.04-2_amd64.tar.zst \
    --hostname ubuntu-llm \
    --cores 4 \
    --memory 8192 \
    --swap 2048 \
    --net0 name=eth0,bridge=vmbr0,ip=dhcp \
    --storage local-lvm \
    --rootfs local-lvm:100 \
    --features nesting=1 \
    --unprivileged 1

# LXC起動
pct start 101
```

#### 3.3 Ubuntu 初期設定

```bash
# LXCにログイン
pct enter 101

# アップデート
apt update && apt upgrade -y

# 必要なパッケージ
apt install -y curl wget git htop
```

### 4. Ollama + Qwen2.5 セットアップ

#### 4.1 Ollama インストール

```bash
# Ollamaインストール
curl -fsSL https://ollama.com/install.sh | sh

# サービス有効化
systemctl enable ollama
systemctl start ollama
```

#### 4.2 Qwen2.5 3B モデル

```bash
# Qwen2.5 3Bをダウンロード（推奨: 日本語対応）
ollama pull qwen2.5:3b

# テスト
ollama run qwen2.5:3b "こんにちは。今日の天気は？"
```

#### 4.3 API設定

```bash
# Ollama APIはデフォルトでlocalhost:11434
# 外部アクセスを許可する場合
echo 'OLLAMA_HOST=0.0.0.0' >> /etc/environment
systemctl restart ollama

# 確認
curl http://localhost:11434/api/tags
```

### 5. Open WebUI (オプション)

```bash
# Docker インストール
apt install -y docker.io docker-compose

# Open WebUI起動
docker run -d \
    --name open-webui \
    --restart always \
    -p 3000:8080 \
    -e OLLAMA_BASE_URL=http://localhost:11434 \
    -v open-webui:/app/backend/data \
    --add-host=host.docker.internal:host-gateway \
    ghcr.io/open-webui/open-webui:main
```

アクセス: `http://192.168.1.xxx:3000`

### 6. Home Assistant 連携

#### 6.1 Ollama 統合

`configuration.yaml`:

```yaml
# Ollama Conversation Agent
conversation:
  - platform: ollama
    url: http://192.168.1.xxx:11434
    model: qwen2.5:3b
    max_history: 10
```

または Home Assistant UI:
1. Settings → Devices & Services → Add Integration
2. "Ollama" を検索
3. URL: `http://192.168.1.xxx:11434`
4. Model: `qwen2.5:3b`

#### 6.2 Whisper (ローカルSTT)

Home Assistant Add-on:
1. Settings → Add-ons → Add-on Store
2. "Whisper" をインストール
3. Configuration:
   ```yaml
   language: ja
   model: base
   beam_size: 5
   ```

#### 6.3 Piper (ローカルTTS)

Home Assistant Add-on:
1. Settings → Add-ons → Add-on Store
2. "Piper" をインストール
3. 日本語音声をダウンロード

#### 6.4 Assist Pipeline 設定

1. Settings → Voice assistants → Add assistant
2. 設定:

| 項目 | 設定値 |
|------|--------|
| Name | ESP32-P4 Voice Assistant |
| Language | Japanese (日本語) |
| Conversation agent | Ollama (qwen2.5:3b) |
| Speech-to-text | Whisper |
| Text-to-speech | Piper |

### 7. Immich セットアップ

#### 7.1 Docker Compose

```bash
# ディレクトリ作成
mkdir -p /opt/immich && cd /opt/immich

# docker-compose.yml をダウンロード
wget https://github.com/immich-app/immich/releases/latest/download/docker-compose.yml
wget -O .env https://github.com/immich-app/immich/releases/latest/download/example.env

# .env を編集
nano .env
# UPLOAD_LOCATION=/mnt/photos
# DB_PASSWORD=your_secure_password
```

```bash
# 起動
docker-compose up -d

# 確認
docker-compose ps
```

アクセス: `http://192.168.1.xxx:2283`

### 8. rclone 設定 (Google Drive 同期)

#### 8.1 インストール

```bash
curl https://rclone.org/install.sh | bash
```

#### 8.2 Google Drive 設定

```bash
rclone config

# 以下の手順:
# n) New remote
# name> gdrive
# Storage> drive
# client_id> (空でOK)
# client_secret> (空でOK)
# scope> 1 (Full access)
# root_folder_id> (空でOK)
# service_account_file> (空でOK)
# Edit advanced config> n
# Use auto config> y (ブラウザで認証)
```

#### 8.3 自動同期 (systemd timer)

`/etc/systemd/system/rclone-sync.service`:

```ini
[Unit]
Description=rclone sync to Google Drive

[Service]
Type=oneshot
ExecStart=/usr/bin/rclone sync /mnt/photos gdrive:Backup/photos --progress
```

`/etc/systemd/system/rclone-sync.timer`:

```ini
[Unit]
Description=Run rclone sync daily

[Timer]
OnCalendar=daily
Persistent=true

[Install]
WantedBy=timers.target
```

```bash
systemctl enable rclone-sync.timer
systemctl start rclone-sync.timer
```

## トラブルシューティング

### Proxmox が起動しない

1. BIOS で VT-x/VT-d を有効化
2. Secure Boot を無効化

### HAOS が起動しない

```bash
# EFI設定を確認
qm set 100 --bios ovmf

# ディスクサイズ拡張
qm resize 100 scsi0 +10G
```

### Ollama のメモリ不足

```bash
# スワップ追加
fallocate -l 4G /swapfile
chmod 600 /swapfile
mkswap /swapfile
swapon /swapfile
echo '/swapfile none swap sw 0 0' >> /etc/fstab
```

### ネットワーク接続問題

```bash
# Proxmox ブリッジ確認
cat /etc/network/interfaces

# HAOSのIP確認
qm terminal 100
# ha network info
```

## 推奨リソース配分

| VM/LXC | CPU | RAM | Disk |
|--------|-----|-----|------|
| Proxmox Host | 2C (予約) | 2GB | - |
| HAOS | 2C | 4GB | 32GB |
| Ubuntu LXC | 4C | 8GB | 100GB |
| **合計** | 6C | 14GB | 132GB |

## 参考リンク

- [Proxmox VE Documentation](https://pve.proxmox.com/wiki/Main_Page)
- [Home Assistant Installation](https://www.home-assistant.io/installation/linux)
- [Ollama Documentation](https://ollama.ai/docs)
- [Immich Documentation](https://immich.app/docs)
- [rclone Documentation](https://rclone.org/docs/)
