# Project Omni-P4: Implementation Plan

## Phase 1: Prototyping (Week 1-2)

### 1.1 Center Tower Prototype
**Goal:** Validate electronics stack before committing to final enclosure

```
Materials:
- 3mm Acrylic sheet (laser cut placeholder for aluminum)
- M3 nylon standoffs (temporary)
- Breadboard or perfboard for sensor mounting

Steps:
1. Cut 3x acrylic plates (80x60, 85x65, 100x80 mm)
2. Drill 4x M3 holes at 75mm pitch on each plate
3. Stack with nylon standoffs
4. Mount all electronics and verify:
   - All sensors communicate via I2C
   - I2S audio path works (DAC → test speaker)
   - Power distribution is adequate
   - Thermal behavior (run 30min test)
```

### 1.2 Software Validation
**Goal:** Confirm all drivers work before mechanical commitment

```
Checklist:
□ ESP32-P4 boots and logs to serial
□ I2C bus scans all 6 sensors
□ SEN0395 (UART1) reports presence data
□ SEN0540 (UART2) recognizes voice commands
□ ES9039 DAC outputs test tone
□ XVF3800 captures audio
□ LCD displays test pattern
```

---

## Phase 2: Detailed CAD Design (Week 3-4)

### 2.1 Software Recommendations

| Software | Purpose | Cost |
|----------|---------|------|
| Fusion 360 | Main CAD, parametric modeling | Free (Hobby) |
| OpenSCAD | Parametric scripting | Free |
| FreeCAD | Alternative to Fusion | Free |
| Prusa Slicer | 3D print preparation | Free |

### 2.2 Design Workflow

```
1. Import dimensions.json into Fusion 360 as parameters
2. Model center tower first (critical dimensions)
3. Model speaker boxes (can be done in parallel)
4. Model outer shell around internal components
5. Add cable routing channels
6. Run interference check
7. Export:
   - STL for 3D printed parts
   - DXF for laser cut parts
   - STEP for CNC shop communication
```

### 2.3 Critical Fit Checks

```
□ LCD fits in front cutout with 2mm tolerance
□ Standoff holes align across all 3 shelf plates
□ Speaker boxes fit within wedge envelope
□ Cable routing doesn't block airflow
□ XVF3800 centered on top surface
□ Bottom intake slots don't interfere with feet
```

---

## Phase 3: Fabrication (Week 5-7)

### 3.1 Ordering Timeline

| Part | Vendor | Lead Time | Notes |
|------|--------|-----------|-------|
| Aluminum plates | Laser cut service | 5-7 days | Include anodizing |
| Aluminum standoffs | Amazon/AliExpress | 3-14 days | Order multiple lengths |
| MDF speaker boxes | Local cabinet shop | 7-10 days | Provide STEP file |
| MDF outer shell | CNC service | 7-10 days | May need multiple pieces |
| 3D printed parts | Self | 2-3 days | PETG for strength |

### 3.2 Recommended Vendors (Japan)

```
Aluminum Laser Cut:
- 株式会社 ミスミ (MISUMI) - fast, precise
- 株式会社 オーエスエム (OSM) - good for small batches
- モノタロウ - wide selection

MDF/Wood CNC:
- Local mokkosha (木工所)
- Makers' Base Tokyo
- FabCafe

3D Print Service (if no printer):
- DMM.make
- JLCPCB (overseas but cheap)
```

### 3.3 Fabrication Checklist

```
□ Aluminum shelf plates (3x) - laser cut + black anodize
□ Aluminum standoffs - ordered or made from rod stock
□ Speaker boxes (2x) - CNC routed, glued, sealed
□ Outer shell pieces - CNC cut, ready for veneer/paint
□ Inner frame - 3D printed, test fit done
□ LCD bezel - 3D printed, sanded, painted
□ Fabric frame - 3D printed
□ Acoustic mesh - cut to size
□ Hanenite pads - cut to size
```

---

## Phase 4: Assembly (Week 8)

### 4.1 Day 1: Mechanical Assembly

```
Morning:
1. Install threaded inserts into 3D printed parts
2. Assemble center tower (plates + standoffs)
3. Test fit speaker boxes in outer shell
4. Install vibration isolation pads

Afternoon:
5. Mount center tower in position
6. Position speaker boxes with correct angles
7. Verify all clearances
8. Take photos for documentation
```

### 4.2 Day 2: Electronics Installation

```
Morning:
1. Mount sensors on Level 1 plate
2. Mount DAC and amp on Level 2 plate
3. Mount ESP32-P4 on Level 3 plate
4. Route power cables (bottom to top)

Afternoon:
5. Connect I2C bus (daisy chain)
6. Connect UART cables
7. Connect I2S cables (shielded)
8. Install LCD and connect MIPI-DSI
9. Install XVF3800 on top
```

### 4.3 Day 3: Final Assembly and Testing

```
Morning:
1. Connect speaker wires
2. Install outer shell panels
3. Install fabric grille
4. Verify all connections

Afternoon:
5. Power on and boot test
6. Run full sensor diagnostic
7. Audio test (all channels)
8. 2-hour burn-in test
9. Final adjustments
```

---

## Phase 5: Quality Assurance

### 5.1 Functional Tests

```
□ All 6 I2C sensors respond correctly
□ SEN0395 detects presence at 3m
□ SEN0540 recognizes "OK Omni" wake word
□ ES9039 produces clean audio (no noise floor issues)
□ XVF3800 captures clear voice (AEC working)
□ LCD displays Home Assistant dashboard
□ Touch input responsive
□ WiFi connects reliably
□ Home Assistant API communication works
```

### 5.2 Thermal Tests

```
Test Conditions:
- Ambient: 25°C
- Load: Continuous audio playback + all sensors active
- Duration: 4 hours

Acceptable Limits:
- ESP32-P4: < 60°C
- DAC/Amp: < 50°C
- Outer shell: < 35°C (touchable)
```

### 5.3 Acoustic Tests

```
□ Speaker frequency response: 80Hz - 20kHz
□ No rattles or buzzing at max volume
□ Passive radiator extends bass to ~60Hz
□ Microphone SNR > 65dB
□ AEC cancellation effective (no feedback loop)
□ Voice recognition works during music playback
```

---

## Phase 6: Documentation and Refinement

### 6.1 Deliverables

```
□ Final CAD files (Fusion 360 archive)
□ STL files for all 3D printed parts
□ DXF files for laser cut parts
□ STEP files for CNC parts
□ Assembly manual with photos
□ Wiring diagram
□ BOM with purchase links
□ Calibration procedures
```

### 6.2 Lessons Learned Template

```markdown
## What Worked Well
-

## What Could Be Improved
-

## Dimensional Changes for V2
-

## Alternative Materials Considered
-
```

---

## Risk Mitigation

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| LCD doesn't fit | Medium | High | Measure actual LCD before final design |
| Thermal issues | Low | Medium | Add extra ventilation if needed |
| Speaker box resonance | Medium | Medium | Add more internal damping |
| I2S noise | Medium | High | Use shielded cables, star ground |
| Assembly difficulty | Low | Low | Create detailed assembly guide |

---

## Budget Summary

| Category | Estimated Cost (¥) |
|----------|-------------------|
| Aluminum fabrication | 4,500 |
| MDF fabrication | 9,000 |
| 3D printing material | 2,000 |
| Fasteners & hardware | 1,700 |
| Acoustic materials | 2,000 |
| **Mechanical Total** | **19,200** |
| | |
| Electronics (from BOM) | ~80,000 |
| | |
| **Grand Total** | **~100,000** |

---

## Timeline Summary

```
Week 1-2:  Prototype center tower, validate electronics
Week 3-4:  Detailed CAD design, order long-lead items
Week 5-7:  Fabrication (parallel with software development)
Week 8:    Assembly
Week 9:    Testing and refinement
Week 10:   Documentation and polish
```

**Target Completion: 10 weeks from start**
