# Home Assistant Voice Assistant Setup Guide

This guide explains how to set up your ESP32-P4 device as a voice assistant in Home Assistant.

## Prerequisites

1. **Home Assistant** version 2023.5 or later
2. **ESPHome Add-on** installed and running
3. **Assist Pipeline** configured in Home Assistant
4. ESP32-P4 device with audio hardware (microphone and speaker)

## Step 1: Install ESPHome Add-on

If you haven't already installed the ESPHome add-on:

1. Go to **Settings** > **Add-ons**
2. Click **Add-on Store**
3. Search for "ESPHome"
4. Click **Install**

## Step 2: Flash Your ESP32-P4 Device

### Option A: Using ESPHome Dashboard

1. Open the ESPHome add-on
2. Click **+ New Device**
3. Enter a name for your device
4. Select **ESP32-P4** as the board type
5. Copy one of the configuration files from `esphome/configs/`:
   - `esp32p4-voice-assistant.yaml` - Full featured
   - `esp32p4-minimal.yaml` - Basic setup
   - `esp32p4-waveshare-nano.yaml` - Waveshare board
   - `esp32p4-smart86box.yaml` - Smart 86 Box panel

### Option B: Command Line

```bash
# Install esphome CLI
pip install esphome

# Compile and flash
esphome run esphome/configs/esp32p4-voice-assistant.yaml
```

## Step 3: Create secrets.yaml

Create a `secrets.yaml` file in your ESPHome directory:

```yaml
wifi_ssid: "YourWiFiSSID"
wifi_password: "YourWiFiPassword"
ap_password: "FallbackPassword"
api_encryption_key: "YOUR_32_CHARACTER_KEY_HERE"
ota_password: "YourOTAPassword"
```

Generate an encryption key:
```bash
esphome generate-encryption-key
```

## Step 4: Add Device to Home Assistant

1. After flashing, the device will appear in **Settings** > **Devices & Services**
2. Click **Configure** on the discovered ESPHome device
3. Enter the encryption key when prompted

## Step 5: Configure Assist Pipeline

### Create a Voice Assistant Pipeline

1. Go to **Settings** > **Voice assistants**
2. Click **Add assistant**
3. Configure:
   - **Name**: ESP32-P4 Voice
   - **Language**: Your preferred language
   - **Conversation agent**: Home Assistant (or your preferred agent)
   - **Speech-to-text**: Whisper or cloud provider
   - **Text-to-speech**: Piper or cloud provider

### Assign Pipeline to Device

1. Go to **Settings** > **Devices & Services** > **ESPHome**
2. Find your ESP32-P4 device
3. Click **Configure**
4. Select the voice assistant pipeline you created

## Step 6: Configure Wake Word

The default wake word is "Okay Nabu". To change it:

### Available Wake Words
- `okay_nabu` (default)
- `hey_jarvis`
- `alexa`
- `hey_mycroft`

### Modify Configuration

```yaml
micro_wake_word:
  models:
    - model: hey_jarvis  # Change wake word here
  on_wake_word_detected:
    - voice_assistant.start:
        wake_word: !lambda return wake_word;
```

## Step 7: Test Your Setup

1. Say the wake word (e.g., "Okay Nabu")
2. The status LED should turn blue (listening)
3. Ask a question or give a command
4. The device will respond through the speaker

## Troubleshooting

### No Response to Wake Word

1. Check WiFi connection in device logs
2. Verify the microphone is working (check `mic_i2s` in logs)
3. Ensure Home Assistant API is connected
4. Check that voice pipeline is properly assigned

### Audio Issues

1. **No sound from speaker**: Check I2S pin configuration
2. **Distorted audio**: Reduce `volume_multiplier` in config
3. **Low volume**: Increase `volume_multiplier` or speaker hardware gain

### Connection Issues

1. **Can't connect to Home Assistant**: Verify API encryption key
2. **WiFi disconnects**: Check signal strength, increase `output_power`
3. **OTA updates fail**: Increase timeout in ESPHome dashboard

### Debug Mode

Enable debug logging in your configuration:

```yaml
logger:
  level: DEBUG
  logs:
    voice_assistant: DEBUG
    micro_wake_word: DEBUG
    i2s_audio: DEBUG
```

## LED Status Indicators

| Color | Pattern | Meaning |
|-------|---------|---------|
| Blue | Solid | Listening |
| Cyan | Solid | Processing speech |
| Yellow | Solid | Waiting for response |
| Purple | Solid | Speaking response |
| Red | Fast pulse | Error occurred |
| Green | Brief flash | Connected to Home Assistant |

## Advanced Configuration

### Custom Intents

Create automations triggered by voice commands:

```yaml
automation:
  - alias: "Voice - Turn on lights"
    trigger:
      - platform: conversation
        command: "turn on the lights"
    action:
      - service: light.turn_on
        target:
          entity_id: light.living_room
```

### Volume Control

Add a volume control entity:

```yaml
number:
  - platform: template
    name: "Voice Volume"
    id: voice_volume
    min_value: 0
    max_value: 100
    step: 10
    optimistic: true
    restore_value: true
    initial_value: 50
    set_action:
      - lambda: |-
          // Adjust speaker volume
          id(speaker_i2s).set_volume(x / 100.0);
```

## Resources

- [ESPHome Voice Assistant Documentation](https://esphome.io/components/voice_assistant.html)
- [Home Assistant Voice Control](https://www.home-assistant.io/voice_control/)
- [ESPHome I2S Audio](https://esphome.io/components/i2s_audio.html)
- [Micro Wake Word](https://esphome.io/components/micro_wake_word.html)
