# Omni-P4: Thermal Management Design

## Heat Sources

| Component | Typical Power | Peak Power | Location |
|-----------|--------------|------------|----------|
| ESP32-P4 | 0.8W | 1.5W | Level 3 |
| ES9039Q2M DAC | 0.3W | 0.5W | Level 2 |
| Peerless Amp | 2W (idle) | 5W (max) | Level 2 |
| DFR1015 Regulator | 1W | 2W | Level 1 |
| Sensors (total) | 0.3W | 0.5W | Level 1 |
| **Total** | **4.4W** | **9.5W** | |

## Thermal Budget

### Ambient Conditions
- Operating range: 10°C ~ 35°C
- Target max surface temp: 40°C (comfortable to touch)
- Max component temp: 70°C (ESP32-P4 Tjmax = 105°C)

### Calculated Temperature Rise
```
Worst case (9.5W, 35°C ambient, natural convection):

ΔT = P × Rth

Where:
  P = 9.5W total dissipation
  Rth ≈ 2.5 °C/W (estimated for enclosed box with vents)

ΔT = 9.5 × 2.5 = 23.75°C

Max component temp = 35 + 24 ≈ 59°C (within limits)
```

---

## Airflow Design

### Convection Strategy: Passive Chimney Effect

```
Side Section View:

                    ┌─── Exhaust ───┐
                    │    (warm)     │
                    │      ↑↑↑      │
         ┌──────────┴───────────────┴──────────┐
         │          XVF3800 Mesh               │
         │              ↑                      │  ← Hot air exits
         ├──────────────┼──────────────────────┤     through mesh
         │   Level 3    │    ESP32-P4          │
         │              │    (1.5W)            │
         │   ═══════════╧═══════════           │  ← 40% open plate
         │              ↑                      │
         │   Level 2    │    DAC + Amp         │
         │              │    (5.5W)            │  ← Hottest zone
         │   ═══════════╧═══════════           │
         │              ↑                      │
         │   Level 1    │    Power + Sensors   │
         │              │    (2.5W)            │
         │   ═══════════╧═══════════           │
         │              ↑                      │
         │              ↑   (cool)             │
         └──────────────┼──────────────────────┘
                        ↑
                    ┌───┴───┐
                    │ Intake│
                    │ Vents │
                    └───────┘
```

### Airflow Path

1. **Intake (Bottom)**
   - 7× slots, 30mm × 5mm each
   - Total open area: 1,050 mm²
   - Located under center tower

2. **Level 1 → Level 2**
   - Through 40% open aluminum plate
   - 4× cable duct channels also allow air

3. **Level 2 → Level 3**
   - Through 40% open aluminum plate
   - Amplifier heat rises naturally

4. **Level 3 → Top**
   - Plate has minimal vents (LCD connector zone)
   - Air exits around mic ring perimeter

5. **Exhaust (Top)**
   - Gap around XVF3800 mesh: ~3mm perimeter
   - Total open area: ~565 mm² (π × 60 × 3)

### Airflow Calculation

```
Estimated natural convection velocity: v ≈ 0.1 m/s

Volumetric flow rate:
  Q = A × v = 0.00105 m² × 0.1 m/s = 0.000105 m³/s = 6.3 L/min

Heat removal capacity:
  P = Q × ρ × Cp × ΔT

  For ΔT = 20°C:
  P = 0.000105 × 1.2 × 1005 × 20 = 2.5W (convection only)

Remaining 7W must be conducted through aluminum plates.
```

---

## Conduction Paths

### Primary Heat Sinks: Aluminum Shelf Plates

```
Thermal Conductivity:
  A6063 Aluminum: k = 200 W/(m·K)

Plate thermal resistance (2mm thick, 50mm path to edge):
  Rth = L / (k × A)
  Rth = 0.05 / (200 × 0.002 × 0.1) = 1.25 °C/W

Heat spreading to outer shell (MDF):
  MDF: k = 0.15 W/(m·K) (poor conductor)

  → Aluminum plates act as heat spreaders
  → Final dissipation through convection at plate edges
```

### Component → Plate Interface

| Component | Interface | Thermal Pad |
|-----------|-----------|-------------|
| ESP32-P4 | Direct mount | 1mm silicone (3 W/m·K) |
| DAC | Standoffs | 1mm silicone |
| Amp heatsink | Thermal tape | 6 W/m·K double-sided |
| Regulator | TO-220 pad | 3mm silicone |

---

## Thermal Design Features

### 1. Ventilation Slots on Plates

```
Level 1 and Level 2 plates:

    ┌────────────────────────────────┐
    │  ▢        ▢        ▢          │   ▢ = Vent slot
    │                                │       (12×8mm)
    │  ●        ●        ●        ● │   ● = Standoff hole
    │                                │
    │  ▢        ▢        ▢          │
    │                                │
    │  ●        ●        ●        ● │
    │                                │
    │  ▢        ▢        ▢          │
    └────────────────────────────────┘

Open area calculation:
  6 slots × (12mm × 8mm) = 576 mm²
  Plate area: 90 × 70 = 6,300 mm²
  Open ratio: 576 / 6,300 = 9.1% (actual)

  + Cable ducts: 4 × (12 × 8) = 384 mm²
  Total open: 960 mm² = 15.2%

Note: 40% target includes mesh-covered areas
```

### 2. Amplifier Heatsink Clearance

```
Amplifier module on Level 2:

    ┌─────────────────────────────┐
    │                             │
    │       ┌───────────┐         │
    │       │           │ Heatsink│  30mm height
    │       │  Amp PCB  │ fins    │
    │       │           │ ← air   │
    │       └───────────┘         │
    │                             │
    └─────────────────────────────┘

Clearance to Level 3 plate: 35mm - 15mm (heatsink) = 20mm
This gap allows horizontal airflow across fins.
```

### 3. Mic Mesh Exhaust

```
XVF3800 mounting ring with exhaust gap:

       ┌────────────────────────┐
       │    Outer shell top     │
       │                        │
       │   ┌────────────────┐   │
       │   │ ▓▓▓ Mesh ▓▓▓▓ │   │  ← 60mm diameter
       │   │  XVF3800 PCB  │   │
       │   └────────────────┘   │
       │   │←── 3mm gap ──→│   │  ← Hot air exits here
       │   │               │   │
       └───┴───────────────┴───┘
```

---

## Speaker Box Thermal Isolation

### Problem
- Amplifier generates up to 5W
- Speaker boxes are MDF (insulator)
- Heat must not conduct to speaker boxes (affects driver suspension)

### Solution
```
                 Amp (heat source)
                       │
                       ▼
    ═══════════════════════════════  ← Level 2 Aluminum Plate
                       │                (heat spreader)
                       │
           ┌───────────┴───────────┐
           │      Air Gap (5mm)    │  ← No direct conduction
           └───────────┬───────────┘
                       │
                 Hanenite Pad (5mm)   ← Rubber = poor conductor
                       │               (k = 0.2 W/m·K)
                       │
    ┌──────────────────┴──────────────────┐
    │          MDF Speaker Box            │
    │                                     │
    │    Temperature rise: < 2°C          │
    └─────────────────────────────────────┘
```

---

## Thermal Testing Protocol

### Test Equipment
- IR thermometer (or thermal camera)
- Ambient thermometer
- Stopwatch

### Test Conditions
1. Ambient: 25°C (room temperature)
2. Load: Continuous music playback at 50% volume
3. All sensors active
4. Duration: 2 hours minimum

### Measurement Points

| Point | Location | Max Temp |
|-------|----------|----------|
| T1 | ESP32-P4 chip center | 60°C |
| T2 | Amplifier heatsink | 55°C |
| T3 | Level 2 plate edge | 45°C |
| T4 | Outer shell (front) | 35°C |
| T5 | Outer shell (top) | 38°C |
| T6 | Speaker box surface | 28°C |

### Pass Criteria
- All points < max temp after 2 hours
- No thermal throttling observed
- Fan not required (passive cooling confirmed)

---

## Contingency: Active Cooling

If passive cooling is insufficient:

### Option A: Low-speed Fan
```
Location: Inside bottom, blowing up
Model: Noctua NF-A4x10 FLX (40mm, 4500 RPM, 17.9 dBA)
Airflow: 8.2 m³/h
Power: 5V, 0.6W
Noise: Barely audible

Mount: Under Level 1 plate, intake side
Control: PWM via ESP32-P4 GPIO, temperature-triggered
```

### Option B: Heatsink Upgrade
```
Component: Amplifier
Current: Internal heatsink only
Upgrade: External aluminum fins (40×40×10mm)
Thermal tape interface
Expected improvement: -10°C on amp
```

### Option C: Vent Enlargement
```
Current bottom vents: 7 × (30×5) = 1,050 mm²
Enlarged vents: 7 × (40×8) = 2,240 mm²

Requires: Outer shell revision
Expected improvement: +50% airflow
```

---

## Summary

| Aspect | Design Choice | Rationale |
|--------|---------------|-----------|
| Cooling method | Passive convection | Silent operation |
| Heat spreader | 2mm A6063 Aluminum | High thermal conductivity |
| Airflow path | Bottom → Top | Natural chimney effect |
| Hottest component | Amplifier (5W) | Positioned mid-tower for best exhaust |
| Speaker isolation | Hanenite + air gap | Prevent thermal effects on drivers |
| Contingency | Optional 40mm fan | Can be added if needed |
