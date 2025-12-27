# Omni-P4: Inner Frame Modular Design

## Overview

インナーフレームは3Dプリント製で、以下の役割を果たします：
1. 外装の曲面と内装の直線構造を繋ぐブリッジ
2. LCDベゼルとして美観を確保
3. スピーカーボックスのマウントブラケット
4. XVF3800マイクのマウントリング

## Module List

```
┌─────────────────────────────────────────────────────────┐
│                    TOP VIEW                              │
│                                                          │
│    ┌──────────┐                      ┌──────────┐       │
│    │ Module D │                      │ Module E │       │
│    │ SPK-L    │    ┌──────────┐      │ SPK-R    │       │
│    │ Bracket  │    │ Module C │      │ Bracket  │       │
│    └──────────┘    │ MIC Ring │      └──────────┘       │
│                    └──────────┘                          │
│         ┌─────────────────────────────┐                  │
│         │        Module B             │                  │
│         │      Tower Base             │                  │
│         └─────────────────────────────┘                  │
│                                                          │
│   ┌─────────────────────────────────────────────────┐   │
│   │                  Module A                        │   │
│   │               LCD Bezel Frame                    │   │
│   └─────────────────────────────────────────────────┘   │
│                                                          │
│                      (FRONT)                             │
└─────────────────────────────────────────────────────────┘
```

---

## Module A: LCD Bezel Frame

### Purpose
- 7インチLCDを前面パネルに固定
- エッジライトの漏れを防止
- 美観の向上（マットブラック仕上げ）

### Specifications
```
Material: ASA (UV resistant, matte black)
Print Orientation: Face down (visible surface up)
Layer Height: 0.15mm (fine finish)
Infill: 100% (solid for rigidity)
Post-process: Light sanding + matte clear coat

Dimensions:
┌────────────────────────────────────────┐
│            190mm (outer)                │
│  ┌──────────────────────────────────┐  │
│  │        172mm (LCD cutout)        │  │
│  │  ┌────────────────────────────┐  │  │
│  │  │                            │  │  │  130mm
│  │  │   Active display visible   │  │  │  (outer)
│  │  │       154 × 86mm           │  │  │
│  │  └────────────────────────────┘  │  │  107mm
│  │                                  │  │  (cutout)
│  └──────────────────────────────────┘  │
│                                        │
│    Thickness: 3mm (bezel) + 8mm (lip)  │
└────────────────────────────────────────┘
```

### Mounting
- M3 heat-set inserts × 4 (corners)
- Clips onto inner frame bracket (snap-fit)

### STL Export Settings
```
Resolution: 0.05mm
Units: mm
Origin: Center of LCD cutout
```

---

## Module B: Tower Base Ring

### Purpose
- 中央タワーを筐体底部に固定
- Level 1棚板の下部サポート
- 配線の整理（ケーブルクリップ付き）

### Specifications
```
Material: PETG (strength + flexibility)
Print Orientation: Flat (ring horizontal)
Layer Height: 0.2mm
Infill: 40% gyroid

Dimensions:
        ┌───────────────────────┐
       /                         \
      /    ┌───────────────┐      \
     │     │   Cable        │      │
     │     │   Through      │      │    100mm
     │     │   Holes ×4     │      │    (outer)
     │     └───────────────┘      │
      \                           /      60mm
       \    4× M3 standoff       /      (inner)
        \   mounting holes      /
         └─────────────────────┘

Height: 10mm
Wall Thickness: 5mm
```

### Features
```
- 4× M3 threaded insert holes for standoff base
- 4× Cable routing channels (12×8mm each)
- 3× Mounting tabs for attachment to outer shell
- Center opening for power cable routing
```

---

## Module C: Microphone Mount Ring

### Purpose
- XVF3800 PCBを天面に水平固定
- 音響メッシュのフレーム
- 配線ダクト（I2Sケーブル）

### Specifications
```
Material: PETG
Print Orientation: Flat
Layer Height: 0.2mm
Infill: 30%

Dimensions:
         ┌─────────────────┐
        /                   \
       /   ┌───────────┐     \
      │    │  PCB Lip   │     │     75mm
      │    │  42×42mm   │     │     (outer)
      │    └───────────┘     │
       \                     /      40mm
        \  Mesh frame      /       (inner)
         └─────────────────┘

Height: 8mm
PCB Lip Depth: 2mm (holds 2mm thick PCB)
```

### Features
```
- Snap-fit PCB retention (4 corners)
- Mesh retention groove (1mm wide × 1mm deep)
- I2S cable channel (4mm × 3mm) to tower
- 3× Mounting posts to outer shell top
```

---

## Module D & E: Speaker Bracket (Left/Right Mirror)

### Purpose
- 19mm MDF スピーカーボックスを固定
- ハネナイト防振パッドのベース
- Toe-in角度（15°）の維持

### Specifications
```
Material: PETG (strong, some flex for vibration isolation)
Print Orientation: Upright (bracket vertical)
Layer Height: 0.2mm
Infill: 60% (structural)

Dimensions:
       ┌─────────────────────┐
       │                     │
       │   ┌─────────────┐   │     180mm
       │   │   MDF Box   │   │     (bracket height)
       │   │   Contact   │   │
       │   │   Area      │   │
       │   └─────────────┘   │
       │         ▼           │
       │    Hanenite pad     │
       │    contact surface  │
       └──────┬───────┬──────┘
              │       │
         (mounting tabs)

Width: 30mm
Depth: 60mm (partial box contact)
```

### Features
```
- 15° angled mounting surface
- Hanenite pad recess (5mm deep)
- 4× M4 through holes for MDF box mounting
- 3× M3 mounting tabs to outer shell
- Cable routing slot for speaker wire
```

---

## Assembly Order

```
1. Print all modules
   └─ Verify dimensions with calipers

2. Install heat-set inserts
   └─ Module A: 4× M3
   └─ Module B: 4× M3
   └─ Module C: None (snap-fit)
   └─ Module D/E: 3× M3 each

3. Pre-assembly test
   └─ Dry fit all modules in outer shell
   └─ Check clearances with center tower
   └─ Verify LCD fits in Module A

4. Install Module B (Tower Base)
   └─ Screw to outer shell bottom
   └─ Feed power cable through center

5. Install Center Tower on Module B
   └─ Align standoffs with M3 holes
   └─ Secure with M3×8 screws

6. Install Modules D & E (Speaker Brackets)
   └─ Position at 15° toe-in
   └─ Screw to outer shell sides

7. Install Speaker Boxes
   └─ Place Hanenite pads in recesses
   └─ Set MDF boxes on pads
   └─ Secure with M4 screws (do not overtighten)

8. Install Module C (Mic Ring)
   └─ Route I2S cable through channel
   └─ Snap XVF3800 PCB into place
   └─ Attach mesh cover

9. Install Module A (LCD Bezel)
   └─ Connect LCD cables
   └─ Clip bezel onto front frame
   └─ Secure with M3 screws if needed
```

---

## 3D Printing Tips

### Material Selection
| Module | Material | Reason |
|--------|----------|--------|
| A | ASA | UV stable, matte finish |
| B | PETG | Strength, cable clip flexibility |
| C | PETG | Snap-fit needs some flex |
| D/E | PETG | Vibration absorption |

### Print Settings (PETG)
```
Nozzle: 0.4mm
Temperature: 240°C nozzle, 80°C bed
Speed: 50mm/s (outer), 80mm/s (infill)
Cooling: 50% after first layer
Supports: Only where needed (minimize)
```

### Post-Processing
```
1. Remove supports carefully
2. Sand mating surfaces with 220 grit
3. Test fit before final assembly
4. Apply matte clear coat to Module A (optional)
```

---

## Part Files

```
/stl/
  ├── module_a_lcd_bezel.stl
  ├── module_b_tower_base.stl
  ├── module_c_mic_ring.stl
  ├── module_d_spk_bracket_left.stl
  └── module_e_spk_bracket_right.stl

/step/
  ├── inner_frame_assembly.step    (complete assembly)
  └── inner_frame_parametric.step  (editable)
```

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2024-01 | Initial design |
| 1.1 | TBD | After prototype testing |
