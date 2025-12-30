# ESP32-P4 Hardware Guide

This guide covers the hardware specifications and pin configurations for various ESP32-P4 development boards.

## ESP32-P4 Overview

The ESP32-P4 is Espressif's high-performance RISC-V application processor designed for multimedia and voice applications.

### Key Specifications

| Feature | Specification |
|---------|--------------|
| CPU | Dual-core RISC-V @ 400 MHz |
| LP Core | Single-core RISC-V @ 40 MHz |
| SRAM | 768 KB HP + 32 KB LP |
| ROM | 128 KB HP + 16 KB LP |
| Flash | Up to 128 MB (external) |
| PSRAM | Up to 64 MB (external) |
| WiFi | Via ESP32-C6 companion (WiFi 6) |
| Bluetooth | Via ESP32-C6 companion (BLE 5) |

### Audio Capabilities

- Multiple I2S interfaces
- I2C for codec control
- Support for ES8311, ES7210, and other codecs
- Built-in audio signal processing
- Echo cancellation support (dual mic)

## Supported Development Boards

### 1. ESP32-P4-Function-EV-Board

Official Espressif evaluation board with comprehensive features.

**Features:**
- ES8311 audio codec
- Dual microphones
- Speaker output
- 7" MIPI-DSI display support
- Camera interface
- ESP32-C6-MINI-1 for WiFi/BT

**Pin Configuration:**

| Function | GPIO |
|----------|------|
| I2S_BCLK | GPIO12 |
| I2S_WS | GPIO13 |
| I2S_DIN | GPIO14 |
| I2S_DOUT | GPIO15 |
| I2C_SDA | GPIO7 |
| I2C_SCL | GPIO8 |
| LED | GPIO47 |
| Button | GPIO0 |

### 2. Waveshare ESP32-P4-NANO

Compact development board with essential features.

**Features:**
- 32MB PSRAM, 16MB Flash
- Onboard speaker and microphone
- MIPI-DSI/CSI interfaces
- USB 2.0 OTG

**Pin Configuration:**

| Function | GPIO |
|----------|------|
| I2S_BCLK | GPIO10 |
| I2S_WS | GPIO11 |
| I2S_DIN | GPIO12 |
| I2S_DOUT | GPIO9 |
| I2C_SDA | GPIO7 |
| I2C_SCL | GPIO8 |
| LED | GPIO48 |
| Button | GPIO0 |

### 3. Smart 86 Box 4" Panel

Wall-mounted touch panel with integrated audio.

**Features:**
- 4" IPS display (720x720)
- 5-point capacitive touch
- WiFi 6 + BLE 5
- Integrated speaker and microphone
- 170° viewing angle

**Pin Configuration:**

| Function | GPIO |
|----------|------|
| I2S_BCLK | GPIO45 |
| I2S_WS | GPIO46 |
| I2S_DIN | GPIO47 |
| I2S_DOUT | GPIO48 |
| I2C_SDA | GPIO7 |
| I2C_SCL | GPIO8 |
| Display BL | GPIO38 |
| Touch INT | GPIO40 |

### 4. ESP32-P4 Pico

Raspberry Pi Pico form factor board.

**Features:**
- 32MB PSRAM, 32MB Flash
- Audio codec on-board
- Speaker terminal
- Dual microphones

## Audio Hardware

### Omni-P4 推奨構成 (Hi-Fi)

高音質を重視する Omni-P4 プロジェクト向けの推奨構成：

| コンポーネント | 型番 | 特徴 |
|--------------|------|------|
| **マイクアレイ** | ReSpeaker USB Mic Array v2.0 | XVF3800、ビームフォーミング |
| **DAC** | ES9038Q2M | 32bit/384kHz、超低歪み |
| **スピーカー** | Peerless PLS-50N25AL02 | 2インチ、ネオジム、4Ω |

### ReSpeaker USB Mic Array v2.0

XMOS XVF3800 ベースの高性能マイクアレイ。USB Audio Class 対応。

**Features:**
- 4マイクアレイ（円形配置）
- 48kHz / 16bit ステレオ出力
- ハードウェアビームフォーミング
- ノイズ抑制・エコーキャンセル
- 12個 RGB LED リング
- USB Audio Class 1.0

**ESP32-P4 接続 (USB Host):**
```
ESP32-P4-Function-EV-Board    ReSpeaker USB
─────────────────────────     ─────────────
USB_HOST_DP (GPIO24)    -->   USB D+
USB_HOST_DM (GPIO25)    -->   USB D-
USB_VBUS_EN (GPIO44)    -->   5V (via MOSFET)
GND                     -->   GND
```

**Dual Buffer Audio Pipeline:**
```
ReSpeaker (48kHz Stereo)
         │
         ▼
┌─────────────────────────┐
│  Audio Pipeline         │
├─────────────────────────┤
│  RAW Buffer (64KB)      │──► Local LLM (高音質)
│  48kHz Stereo           │
├─────────────────────────┤
│  Processed Buffer (16KB)│──► ESPHome/HA
│  16kHz Mono             │    (Whisper対応)
└─────────────────────────┘
```

### ES9038Q2M DAC

ESS Technology の高性能 DAC チップ搭載モジュール。

**Features:**
- 32bit / 384kHz 対応
- THD+N: -120dB
- DNR: 129dB
- I2S / DSD 入力
- 低ジッタークロック内蔵

**ESP32-P4 接続 (I2S0):**
```
ESP32-P4              ES9038Q2M Module
────────              ────────────────
GPIO11 (MCLK)   -->   MCLK (Master Clock)
GPIO12 (BCK)    -->   BCK (Bit Clock)
GPIO13 (WS)     -->   LRCK (Word Select)
GPIO14 (DOUT)   -->   DIN (Data Input)
3.3V            -->   VCC
GND             -->   GND
```

**推奨設定:**
```c
// I2S0 Configuration for ES9038Q2M
#define I2S0_SAMPLE_RATE    48000   // or 96000, 192000
#define I2S0_BIT_WIDTH      32
#define I2S0_MCLK_MULTIPLE  256     // MCLK = 256 * Fs
```

### Peerless PLS-50N25AL02 スピーカー

高品質2インチ ネオジムフルレンジスピーカー。

**Specifications:**
- サイズ: 50mm (2インチ)
- インピーダンス: 4Ω
- 許容入力: 10W (RMS)
- 周波数特性: 120Hz - 20kHz
- 感度: 82dB/W/m
- マグネット: ネオジム

**アンプ接続:**
推奨アンプ: PAM8403 (3W x 2) または TPA3116D2 (15W x 2)

```
ES9038Q2M             PAM8403              Speaker
─────────             ───────              ───────
LOUT  ───────────►    LIN      LOUT+ ───► (+)
                               LOUT- ───► (-)
ROUT  ───────────►    RIN      ROUT+ ───► (+)
                               ROUT- ───► (-)
                      VCC ◄─── 5V
                      GND ◄─── GND
```

### ES8311 Audio Codec (レガシー)

ES8311 は ESP32-P4-Function-EV-Board に標準搭載されているコーデック。
ESPHome 基本構成で使用。

**Features:**
- 8kHz to 96kHz sample rates
- 24-bit ADC/DAC
- Integrated headphone amplifier
- Low power consumption (1.8mW typical)
- I2C control interface (address: 0x18)

**Connection:**
```
ESP32-P4         ES8311
---------        ------
I2C_SDA    -->   SDA
I2C_SCL    -->   SCL
I2S_BCLK   -->   SCLK
I2S_WS     -->   LRCK
I2S_DIN    <--   SDOUT (ADC output)
I2S_DOUT   -->   SDIN (DAC input)
MCLK       -->   MCLK (optional, can use internal)
```

### Microphone Options

#### INMP441 I2S Microphone
- Digital I2S output
- 61 dB SNR
- Low power (1.4mW)

**Connection:**
```
ESP32-P4         INMP441
---------        -------
I2S_BCLK   -->   SCK
I2S_WS     -->   WS
I2S_DIN    <--   SD
3.3V       -->   VDD
GND        -->   GND, L/R (for left channel)
```

#### PDM Microphone
Some boards use PDM microphones instead of I2S.

**Configuration for PDM:**
```yaml
microphone:
  - platform: i2s_audio
    id: mic_pdm
    pdm: true
    i2s_din_pin: GPIO14
    channel: left
```

### Speaker/Amplifier Options

#### MAX98357A I2S Amplifier
- 3W Class D amplifier
- I2S input
- No MCLK required

**Connection:**
```
ESP32-P4         MAX98357A
---------        ---------
I2S_BCLK   -->   BCLK
I2S_WS     -->   LRCLK
I2S_DOUT   -->   DIN
3.3V       -->   VIN
GND        -->   GND
           -->   GAIN (configure gain)
```

## Power Considerations

### Power Supply Requirements

- Main supply: 3.3V @ 500mA minimum
- With PSRAM: 3.3V @ 800mA recommended
- With display: 5V @ 1A recommended

### Power Optimization

For battery-powered applications:

```yaml
wifi:
  power_save_mode: light  # Use light instead of none

esp32:
  framework:
    sdkconfig_options:
      CONFIG_ESP32P4_DEFAULT_CPU_FREQ_240: y  # Lower CPU frequency
```

## Custom Board Configuration

To configure a custom board, modify the pin definitions in your YAML:

```yaml
substitutions:
  # I2S Audio pins
  i2s_bclk_pin: GPIO12    # Bit Clock
  i2s_ws_pin: GPIO13      # Word Select (LRCLK)
  i2s_din_pin: GPIO14     # Data In (from microphone)
  i2s_dout_pin: GPIO15    # Data Out (to speaker)

  # I2C pins (for codec control)
  i2c_sda_pin: GPIO7
  i2c_scl_pin: GPIO8

  # Status LED
  led_pin: GPIO47

  # User button
  button_pin: GPIO0
```

## Debugging Hardware Issues

### I2S Signal Analysis

Use a logic analyzer to verify I2S signals:
- BCLK: Continuous clock during audio
- WS: Frame sync, toggles each sample
- DIN/DOUT: Data aligned with clock

### I2C Scanner

Add this to verify codec communication:

```yaml
i2c:
  sda: ${i2c_sda_pin}
  scl: ${i2c_scl_pin}
  scan: true  # Will log detected devices
```

### Common Issues

1. **No audio input**: Check microphone orientation (some have specific up/down)
2. **Distorted output**: Verify power supply can handle speaker load
3. **Codec not found**: Check I2C address and pull-up resistors
4. **Interference/noise**: Add decoupling capacitors near audio components
