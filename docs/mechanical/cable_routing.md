# Omni-P4: Cable Routing Specification

## Overview

中央タワーの4隅に配置されたケーブルダクトを通じて、各階層間で信号と電源を配線します。

```
Top View of Cable Duct Layout:

           FRONT
    ┌─────────────────────┐
    │                     │
    │  [A]           [B]  │    [A] = I2S + Audio
    │   ●─────────────●   │    [B] = I2C + UART
    │   │   Plate     │   │    [C] = Power (12V, 5V)
    │   │   Area      │   │    [D] = Speaker + Ground
    │   ●─────────────●   │
    │  [C]           [D]  │
    │                     │
    └─────────────────────┘
           BACK
```

---

## Duct Specifications

### Physical Dimensions
```
Each corner duct:
- Width: 12mm
- Depth: 8mm
- Located: 3mm from plate edge

Cross-section:
    ┌──────────────┐
    │              │  8mm
    │   Cables     │
    │              │
    └──────────────┘
         12mm
```

### Cable Capacity per Duct
| Duct | Max Cables | Reserved For |
|------|------------|--------------|
| A | 3 | I2S (shielded), Audio signals |
| B | 4 | I2C, UART1, UART2 |
| C | 2 | 12V, 5V power |
| D | 3 | Speaker wires, GND |

---

## Cable Types and Routing

### 1. I2S Cables (Shielded)

**I2S0 (DAC Output): Level 3 → Level 2**
```
Route: Duct A (front-left)
Cable: 4-wire shielded (AWG 26)
Length: 50mm
Conductors:
  - BCLK (GPIO 12)
  - WS (GPIO 10)
  - DOUT (GPIO 9)
  - MCLK (GPIO 13)
  - Shield → Level 2 plate GND
```

**I2S1 (Mic Input): Top → Level 3**
```
Route: Duct A, then to mic ring
Cable: 4-wire shielded (AWG 26)
Length: 80mm
Conductors:
  - BCLK (GPIO 45)
  - WS (GPIO 46)
  - DIN (GPIO 47)
  - Shield → ESP32-P4 GND
```

### 2. I2C Bus (Daisy Chain)

**Sensor Bus: Level 1 → Level 3**
```
Route: Duct B (front-right)
Cable: 4-wire ribbon (AWG 28)
Length: 100mm (with slack)
Conductors:
  - SDA (GPIO 7)
  - SCL (GPIO 8)
  - VCC (3.3V)
  - GND

Daisy Chain Order (Level 1):
  ESP32-P4 → SCD41 → SGP40 → ENS160 → BMP388 → SHT40 → SEN0428
            ↓        ↓        ↓        ↓        ↓        ↓
          0x62     0x59     0x52     0x77     0x44     0x19
```

### 3. UART Cables

**UART1 (SEN0395 Radar): Level 1 → Level 3**
```
Route: Duct B
Cable: 3-wire (AWG 26)
Length: 90mm
Conductors:
  - TX (GPIO 17)
  - RX (GPIO 18)
  - GND
```

**UART2 (SEN0540 Voice): Level 1 → Level 3**
```
Route: Duct B
Cable: 3-wire (AWG 26)
Length: 90mm
Conductors:
  - TX (GPIO 4)
  - RX (GPIO 5)
  - GND
```

### 4. Power Cables

**12V Input: Bottom → Level 1 → Level 2**
```
Route: Duct C (back-left)
Cable: 2-wire (AWG 20, red/black)
Length: 150mm total
Path:
  DC Jack → WAGO 221 (Level 1) → Amp (Level 2)
```

**5V Distribution: Level 1 → All Levels**
```
Route: Duct C
Cable: 2-wire (AWG 22, orange/black)
Length: 80mm per segment
Path:
  DFR1015 output → WAGO 221 → ESP32-P4 (Level 3)
                            → Sensors (Level 1)
```

### 5. Speaker Wires

**Left Speaker: Level 2 → Speaker Box**
```
Route: Duct D (back-right) → exit at Level 2
Cable: 2-conductor (AWG 18, stranded)
Length: 200mm
Path:
  Amp L+ L- → Through Duct D → Exit side → Speaker box
```

**Right Speaker: Level 2 → Speaker Box**
```
Route: Duct D → exit at Level 2
Cable: 2-conductor (AWG 18, stranded)
Length: 200mm
Path:
  Amp R+ R- → Through Duct D → Exit side → Speaker box
```

---

## Routing Diagram (Side Section)

```
                    XVF3800 (Mic)
                        │
           ┌────────────┼────────────┐ ← I2S1 to ESP32-P4
           │            │            │
    ═══════╪════════════╪════════════╪═══════ Level 3 (ESP32-P4)
           │            │            │
         [A]          [B]          [C]
           │            │            │
           │ I2S0       │ I2C        │ 5V
           │            │ UART1/2    │
           ▼            ▼            ▼
    ═══════╪════════════╪════════════╪═══════ Level 2 (DAC/Amp)
           │            │            │
           │            │            │ 12V
           │            │            │
           ▼            ▼            ▼
    ═══════╪════════════╪════════════╪═══════ Level 1 (Power/Sensors)
           │            │            │
           │            │            │
           ▼            ▼            ▼
    ───────┴────────────┴────────────┴─────── Bottom (DC Jack)

    [A] = I2S audio signals
    [B] = I2C + UART data
    [C] = Power distribution
    [D] = (not shown) Speaker wires exit at Level 2
```

---

## Cable Management Accessories

### 1. Cable Clips (3D Printed)
```
Type: Snap-fit clip
Material: PETG
Size: 8mm × 5mm × 10mm
Quantity: 12 (3 per duct level)

Attachment: Slide onto plate edge
```

### 2. Strain Relief
```
Location: Where cables exit tower to speakers
Type: Rubber grommet
Size: ID 6mm, OD 10mm
Purpose: Prevent cable damage, vibration isolation
```

### 3. Cable Ties
```
Type: Velcro (reusable)
Width: 10mm
Length: 100mm
Use: Bundle cables at each level
```

---

## EMI/Noise Considerations

### Shielded Cable Requirements
| Cable Type | Shielding | Reason |
|------------|-----------|--------|
| I2S0/I2S1 | Yes (braid) | Audio quality |
| I2C | Optional | Low speed, short runs |
| UART | No | Low speed |
| Power | No | DC only |
| Speaker | No | Low impedance |

### Separation Rules
```
1. Keep I2S cables minimum 15mm from power cables
2. Route audio and data on opposite sides (A vs C)
3. Ground shields at ONE end only (ESP32-P4 side)
4. Cross power and signal at 90° if unavoidable
```

### Ground Topology
```
                    ┌─── Level 3 Plate ───┐
                    │       (GND)          │
                    │         │            │
                    │    ESP32-P4 GND      │
                    │    (star point)      │
                    │     ╱  │  ╲          │
                    │    ╱   │   ╲         │
                    └───╱────│────╲────────┘
                       ╱     │     ╲
                      ╱      │      ╲
            I2S Shield   I2C GND   Power GND
                  │         │         │
         ┌────────┴─────────┴─────────┴────────┐
         │            Level 2 Plate             │
         │              (GND)                   │
         │                │                     │
         │            Amp GND                   │
         └────────────────┴─────────────────────┘
                          │
         ┌────────────────┴─────────────────────┐
         │            Level 1 Plate             │
         │              (GND)                   │
         │         ╱     │     ╲                │
         │    Sensor   WAGO    Chassis          │
         │      GND    GND     GND              │
         └────────────────┴─────────────────────┘
                          │
                     DC Jack GND
```

---

## Cable Length Summary

| Cable | From | To | Length | AWG |
|-------|------|-----|--------|-----|
| I2S0 | ESP32-P4 | DAC | 50mm | 26 |
| I2S1 | XVF3800 | ESP32-P4 | 80mm | 26 |
| I2C bus | ESP32-P4 | Sensors | 100mm | 28 |
| UART1 | ESP32-P4 | SEN0395 | 90mm | 26 |
| UART2 | ESP32-P4 | SEN0540 | 90mm | 26 |
| 12V | DC Jack | WAGO | 60mm | 20 |
| 12V | WAGO | Amp | 40mm | 20 |
| 5V | DFR1015 | WAGO | 30mm | 22 |
| 5V | WAGO | ESP32-P4 | 80mm | 22 |
| Speaker L | Amp | Spk Box | 200mm | 18 |
| Speaker R | Amp | Spk Box | 200mm | 18 |

**Total Cable Length: ~920mm**

---

## Bill of Materials (Cables)

| Item | Quantity | Unit | Notes |
|------|----------|------|-------|
| 4-wire shielded (AWG26) | 150 | mm | I2S cables |
| 4-wire ribbon (AWG28) | 100 | mm | I2C bus |
| 3-wire (AWG26) | 180 | mm | UART cables |
| 2-wire (AWG20, red/black) | 100 | mm | 12V power |
| 2-wire (AWG22, orange/black) | 200 | mm | 5V power |
| 2-conductor (AWG18) | 400 | mm | Speaker wire |
| JST-XH connectors | 8 | pcs | Signal connections |
| Spade terminals | 4 | pcs | Speaker connections |
| Heat shrink assorted | 1 | set | Cable finishing |
