# Project Omni-P4: Mechanical Design Specification

## 1. Overall Dimensions (Formation Wedge Style)

### 1.1 External Envelope
```
        ┌───────────────── 380mm ─────────────────┐
        │                                         │
   ▲    ╔═══════════════════════════════════════╗ ─┐
   │    ║              Front Face               ║  │
   │    ║    ┌─────────────────────────┐       ║  │
 180mm  ║    │   7" LCD (170×105mm)   │       ║  │ 200mm
   │    ║    │   Active: 154×86mm     │       ║  │ (depth)
   │    ║    └─────────────────────────┘       ║  │
   ▼    ╚═══════════════════════════════════════╝ ─┘

        Top View (120° Wedge):

                    Front (380mm)
              ┌─────────────────────┐
             /                       \
            /         120°            \
           /                           \
          /                             \
         /     ┌───────────────┐        \
        /      │  Center Tower │         \
       /       │    (100mm)    │          \
      /        └───────────────┘           \
     /    ┌────┐             ┌────┐         \
    /     │ L  │             │ R  │          \
   /      │SPK │             │SPK │           \
  /       └────┘             └────┘            \
 └──────────────────────────────────────────────┘
                  Back (~150mm)
```

### 1.2 Key Dimensions
| Parameter | Value | Notes |
|-----------|-------|-------|
| Width (front) | 380mm | Formation Wedge inspired |
| Width (back) | ~150mm | Calculated from 120° angle |
| Depth | 200mm | Center depth |
| Height | 180mm | Including feet |
| Wedge angle | 120° | Symmetric |

---

## 2. Central Electronics Tower

### 2.1 Inverted Pyramid Stack Structure
```
                    Top (Widest)
    ┌──────────────────────────────────────┐
    │  LEVEL 3: LCD + ESP32-P4             │  ← 100mm × 80mm
    │  Height: 35mm                        │
    ├──────────────────────────────────────┤
    │        ↑ Aluminum Standoffs ↑        │
    ├────────────────────────────────┤
    │  LEVEL 2: ES9039Q2M DAC + Amp  │       ← 90mm × 70mm
    │  Height: 30mm                  │
    ├────────────────────────────────┤
    │        ↑ Aluminum Standoffs ↑  │
    ├──────────────────────────┤
    │  LEVEL 1: Power + Sensors │            ← 80mm × 60mm
    │  Height: 40mm             │
    └──────────────────────────┘
              Bottom (Narrowest)
```

### 2.2 Aluminum Standoff Specifications
```
Material: A6063 Aluminum (Black Anodized)
Thread: M3 (Female-Female)
Outer Diameter: 6mm (Hex or Round)

Standoff Heights:
- Level 1→2: 40mm (Power components clearance)
- Level 2→3: 35mm (DAC board clearance)
- Level 3→Top: 25mm (LCD mounting)

Total Tower Height: 100mm (internal) + 25mm (LCD) = 125mm
```

### 2.3 Aluminum Shelf Plates
```
Material: A6063-T5 Aluminum
Thickness: 2.0mm
Surface: Black Anodized

Level 1 Plate (Bottom):
  Size: 80mm × 60mm
  Holes: 4× M3 corner mounting + cable routing slots
  Features: Ventilation cutouts (40% open area)

Level 2 Plate (Middle):
  Size: 90mm × 70mm
  Holes: 4× M3 mounting + I2S ribbon cable slot
  Features: DAC board thermal pad contact area

Level 3 Plate (Top):
  Size: 100mm × 80mm
  Holes: 4× M3 + LCD connector clearance
  Features: ESP32-P4 mounting + USB port access
```

### 2.4 Standoff Position Layout
```
Reference: 7" LCD VESA-like mounting (75mm × 75mm typical)

       ┌────────────100mm────────────┐
       │                             │
   ▲   │  ●─────────75mm──────────●  │  ← M3 Standoff positions
   │   │  │                       │  │
   │   │  │                       │  │
 80mm  │  75mm                    │  │
   │   │  │                       │  │
   │   │  │                       │  │
   ▼   │  ●───────────────────────●  │
       │                             │
       └─────────────────────────────┘

Standoff Coordinates (Origin = Center):
  A: (-37.5, -37.5)
  B: (+37.5, -37.5)
  C: (+37.5, +37.5)
  D: (-37.5, +37.5)
```

---

## 3. Speaker Enclosures (Left/Right)

### 3.1 MDF Box Specifications
```
Material: MDF (Medium Density Fiberboard)
Thickness: 19mm
Internal Treatment: Acoustic dampening foam
External Treatment: Veneer or paint

Internal Volume Target: 2.0L per channel (minimum)
Actual Internal Dimensions:
  Width:  80mm
  Height: 120mm
  Depth:  180mm
  Volume: 80 × 120 × 180 = 1,728,000 mm³ = 1.73L

Note: Add 10% for port/passive radiator compensation
```

### 3.2 Speaker Layout
```
Front View (per speaker box):

    ┌────────────────────────────┐
    │                            │
    │    ┌────────────────┐     │
    │    │   Peerless     │     │  ← Main Driver
    │    │   Full Range   │     │     Ø 65mm cutout
    │    │   (2.5-3")     │     │
    │    └────────────────┘     │
    │                            │
    │    ┌────────────────┐     │
    │    │   Passive      │     │  ← Passive Radiator
    │    │   Radiator     │     │     Ø 65mm cutout
    │    │   (Dayton)     │     │     15mm stroke clearance
    │    └────────────────┘     │
    │                            │
    └────────────────────────────┘

Side Section View:
                          ← 19mm MDF
    ┌─┬────────────────────┬─┐
    │ │ Dampening Foam     │ │
    │ │                    │ │
    │ │   ┌──────────┐     │ │  ← Internal cavity
    │ │   │ Air Vol. │     │ │
    │ │   │  1.7L    │     │ │
    │ │   └──────────┘     │ │
    │ │                    │ │
    └─┴────────────────────┴─┘
      │←────── 180mm ──────→│
```

### 3.3 Vibration Isolation
```
Anti-Vibration Mount: Hanenite (ハネナイト) rubber
Hardness: Shore A 40-50
Thickness: 5mm
Placement: Between MDF box and inner frame

    MDF Speaker Box
    ═══════════════
         ▓▓▓▓▓      ← Hanenite (5mm)
    ───────────────
    Inner Frame (3D Printed)
```

---

## 4. LCD and Front Panel

### 4.1 7-inch LCD Specifications
```
Display Type: IPS LCD with Capacitive Touch
Resolution: 1024 × 600 (assumed)
Active Area: 154mm × 86mm
Module Size: ~170mm × 105mm (with PCB)
Connector: MIPI-DSI + I2C Touch

Bezel Design:
    ┌─────────────────────────────────────┐
    │  ████████████████████████████████  │  ← 10mm bezel (top)
    │  █┌─────────────────────────────┐█  │
    │  █│                             │█  │
    │  █│     Active Display Area     │█  │  ← 154 × 86mm
    │  █│        1024 × 600           │█  │
    │  █│                             │█  │
    │  █└─────────────────────────────┘█  │
    │  ████████████████████████████████  │  ← 15mm bezel (bottom)
    └─────────────────────────────────────┘

Total Bezel Size: ~190mm × 130mm
```

### 4.2 Front Fabric Grille
```
Material: Acoustically transparent fabric (Guilford of Maine or equivalent)
Frame: 3D printed ABS/ASA with fabric tension clips
Coverage: Full front face except LCD window

    ┌─────────────────────────────────────────┐
    │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│  ← Fabric covered
    │▓▓▓▓┌─────────────────────────┐▓▓▓▓▓▓▓▓│
    │▓▓▓▓│                         │▓▓▓▓▓▓▓▓│
    │▓▓▓▓│      LCD Window         │▓▓▓▓▓▓▓▓│
    │▓▓▓▓│     (clear opening)     │▓▓▓▓▓▓▓▓│
    │▓▓▓▓└─────────────────────────┘▓▓▓▓▓▓▓▓│
    │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
    └─────────────────────────────────────────┘
```

---

## 5. Microphone Array Placement

### 5.1 XVF3800 Position (Top Surface)
```
Optimal DoA (Direction of Arrival) requires:
- Horizontal placement on top surface
- Minimal acoustic obstructions
- Centered for symmetric pickup

Top View:
    ┌─────────────────────────────────────────┐
    │                                         │
    │         ┌───────────────┐               │
    │         │   XVF3800     │               │  ← Centered on top
    │         │   4-mic PCB   │               │
    │         │   (~40×40mm)  │               │
    │         └───────────────┘               │
    │                                         │
    │    Mic 1 ●           ● Mic 2            │  ← 4 MEMS mics
    │                                         │     in square array
    │    Mic 4 ●           ● Mic 3            │
    │                                         │
    └─────────────────────────────────────────┘

Acoustic Mesh:
- Material: Stainless steel 100-mesh
- Diameter: 60mm circular opening
- Purpose: Dust protection while maintaining acoustic transparency
```

---

## 6. Thermal Management

### 6.1 Airflow Path
```
Side Section View:

                    Exhaust (Top)
                        ↑↑↑
    ┌───────────────────┬───────────────────┐
    │                   │ XVF3800           │
    │                   ├───────────────────┤
    │  Speaker          │ LCD + ESP32-P4    │  ← Warm zone
    │  Chamber          ├───────────────────┤
    │  (Isolated)       │ DAC + Amp         │  ← Hot zone (5W)
    │                   ├───────────────────┤
    │                   │ Power + Sensors   │  ← Warm zone
    └───────────────────┴───────────────────┘
                        ↑↑↑
                    Intake (Bottom)

Ventilation Slots:
- Bottom intake: 8× slots, 5mm × 40mm each
- Top exhaust: Perimeter gap around XVF3800 mesh
- Shelf plates: 40% open area with strategic cutouts
```

### 6.2 Heat Sources and Dissipation
| Component | Power | Solution |
|-----------|-------|----------|
| ESP32-P4 | 1.5W max | Aluminum shelf conduction |
| ES9039Q2M DAC | 0.5W | Direct shelf contact |
| Amplifier | 5W (2×2.5W) | Thermal pad to shelf |
| Power regulators | 2W | Airflow + shelf |

---

## 7. Cable Routing

### 7.1 Internal Wiring Diagram
```
Power Distribution:
    12V DC Jack
        │
        ▼
    ┌─────────┐
    │ DFR1015 │ ────────┬────────────→ Amp (12V)
    │ 12V→5V  │         │
    └────┬────┘    ┌────┴────┐
         │         │ WAGO 221│ ← Distribution block
         │         └────┬────┘
         │              │
         ▼              ▼
    ESP32-P4 (5V)   Sensors (5V/3.3V)

Signal Routing:
    ┌───────────────────────────────────────────┐
    │                Level 3                    │
    │  ESP32-P4 ←─I2S0─→ DAC                   │
    │      │      ←─I2S1─→ (to Level 4)        │
    │      │      ←─I2C──→ (down to L1)        │
    │      └──UART1/2──→ (to L1 sensors)       │
    ├───────────────────────────────────────────┤
    │                Level 2                    │
    │  DAC ←──Speaker Wire──→ Speaker L/R      │
    ├───────────────────────────────────────────┤
    │                Level 1                    │
    │  Sensors ←──I2C bus──→ (up to L3)        │
    │  SEN0395 ←──UART1──→ (up to L3)          │
    │  SEN0540 ←──UART2──→ (up to L3)          │
    └───────────────────────────────────────────┘
```

### 7.2 Cable Management
```
Cable Types:
- I2S: 4-wire ribbon (BCLK, WS, DATA, GND) - Shielded
- I2C: 4-wire (SDA, SCL, VCC, GND) - 10cm max
- UART: 3-wire (TX, RX, GND) per device
- Speaker: 18AWG stranded, 15cm length
- Power: 20AWG, star ground topology
```

---

## 8. Manufacturing Notes

### 8.1 3D Printed Parts
| Part | Material | Infill | Notes |
|------|----------|--------|-------|
| Inner frame | PETG/ASA | 40% | Structural |
| LCD bezel | ASA | 100% | Visible, matte black |
| Fabric frame | PLA | 20% | Hidden |
| Cable clips | PETG | 30% | Functional |

### 8.2 CNC/Laser Cut Parts
| Part | Material | Thickness | Service |
|------|----------|-----------|---------|
| Shelf plates | A6063 Al | 2mm | Laser cut + anodize |
| Side panels | Plywood | 4mm | CNC router |
| Outer shell | MDF | 6mm | CNC + veneer |

### 8.3 Speaker Box
- Professional cabinet shop recommended
- CNC routing for driver cutouts
- Precision matters for air-tight seal

---

## 9. Assembly Sequence

1. **Assemble Center Tower**
   - Mount standoffs to Level 1 plate
   - Stack Level 2 and 3 plates
   - Verify vertical alignment

2. **Install Electronics**
   - Mount sensors on Level 1
   - Mount DAC/Amp on Level 2
   - Mount ESP32-P4 on Level 3

3. **Wire Tower**
   - Route power cables (bottom-up)
   - Connect I2C bus
   - Connect I2S cables

4. **Prepare Speaker Boxes**
   - Install drivers and passive radiators
   - Wire speakers (leave tails)
   - Apply vibration damping

5. **Assemble Inner Frame**
   - Insert center tower
   - Position speaker boxes with Hanenite
   - Secure with mounting brackets

6. **Install LCD**
   - Mount LCD to bezel
   - Connect MIPI-DSI cable
   - Attach bezel to top frame

7. **Install XVF3800**
   - Mount on top surface
   - Route I2S cable to ESP32-P4
   - Install acoustic mesh

8. **Final Assembly**
   - Attach outer shell panels
   - Install fabric grille
   - Final wiring check

---

## 10. Bill of Materials Summary

### Custom Fabricated Parts
| Qty | Part | Material | Est. Cost |
|-----|------|----------|-----------|
| 3 | Shelf plates | 2mm A6063 Al | ¥3,000 |
| 12 | M3 standoffs | Al (40/35/25mm) | ¥1,500 |
| 2 | Speaker boxes | 19mm MDF | ¥5,000 |
| 1 | Outer shell | 6mm MDF | ¥4,000 |
| 1 | Inner frame | PETG 3D print | ¥2,000 |

### Hardware
| Qty | Part | Est. Cost |
|-----|------|-----------|
| 24 | M3×8 screws | ¥500 |
| 8 | M3 threaded inserts | ¥400 |
| 4 | Hanenite pads | ¥800 |
| 1 | Acoustic fabric | ¥1,500 |
| 1 | Steel mesh | ¥500 |

**Total Mechanical BOM: ~¥19,200**
