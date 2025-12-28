// ============================================================================
// Project Omni-P4: Parametric Design v4.2
// Updated for Large ESP32-P4 EV Board (74×88mm)
// ============================================================================
//
// KEY CHANGES FROM v4.1:
// - STANDOFF_PITCH: 55mm → 90mm (for 74mm wide EV Board)
// - TOWER_TOP_W: 100mm → 120mm
// - Level 3: L-shaped slots for position adjustment
// - Level 2: Universal slot grid for any DAC/Amp
// - Level 1: Universal slot grid + large airflow opening
// - Top plate: ReSpeaker hole + LED diffuser recess
//
// ============================================================================

$fn = 96;

// ============================================================================
// MASTER PARAMETERS v4.2
// ============================================================================

// === Outer Dimensions (B&W Trace v4.1 Base) ===
FRONT_WIDTH       = 440;      // mm
DEPTH             = 220;      // mm (updated from 243)
HEIGHT            = 232;      // mm
SECTOR_ANGLE      = 120;      // degrees

// === Shell Construction ===
MDF_THICKNESS     = 12;       // 12mm MDF outer shell
SHELL_THICKNESS   = MDF_THICKNESS;

// === Center Tower (Updated for large EV Board) ===
STANDOFF_PITCH    = 90;       // M3 standoff spacing (was 55mm, now 90mm for 74mm board)
STANDOFF_OD       = 6;        // Standoff outer diameter
STANDOFF_ID       = 3;        // M3 thread

TOWER_TOP_W       = 120;      // Top plate width (for ESP32-P4 EV Board)
TOWER_BASE_W      = 100;      // Base plate width

// === Plate Dimensions ===
PLATE_THICKNESS   = 2.0;      // Aluminum plate

// Level 3: ESP32-P4 EV Board (74×88mm)
PLATE_L3_W        = 120;      // Wide enough for 74mm + L-slot adjustment
PLATE_L3_D        = 100;      // Deep enough for 88mm board

// Level 2: DAC + Amp (Universal)
PLATE_L2_W        = 110;
PLATE_L2_D        = 95;

// Level 1: Power + Sensors
PLATE_L1_W        = 100;
PLATE_L1_D        = 85;

// === Universal Slot Grid Parameters ===
SLOT_WIDTH        = 4;        // 4mm wide slots
SLOT_LENGTH       = 15;       // 15mm long slots
SLOT_SPACING_X    = 20;       // X grid spacing
SLOT_SPACING_Y    = 18;       // Y grid spacing

// === ESP32-P4 EV Board Dimensions ===
EV_BOARD_W        = 74;       // mm
EV_BOARD_D        = 88;       // mm
EV_BOARD_HOLES    = [[3, 3], [71, 3], [71, 85], [3, 85]]; // Mounting holes

// L-slot dimensions for position adjustment
L_SLOT_LENGTH     = 12;       // Long arm of L
L_SLOT_WIDTH      = 6;        // Width of slot
L_SLOT_SHORT      = 8;        // Short arm of L

// === Cable Routing Hole ===
CABLE_HOLE_W      = 40;       // mm
CABLE_HOLE_D      = 20;       // mm

// === Airflow Opening (Level 1) ===
AIRFLOW_W         = 50;       // mm
AIRFLOW_D         = 40;       // mm

// === ReSpeaker & LED Diffuser ===
RESPEAKER_DIA     = 72;       // ReSpeaker PCB diameter + clearance
LED_RING_OD       = 68;       // LED diffuser outer diameter
LED_RING_ID       = 50;       // LED diffuser inner diameter
DIFFUSER_DEPTH    = 3;        // Recess depth for diffuser

// Tower level heights
LEVEL_1_Z         = 25;       // Power + sensors
LEVEL_2_Z         = 80;       // DAC + Amp
LEVEL_3_Z         = 135;      // ESP32-P4

// Standoff heights between levels
STANDOFF_L1_L2    = 53;       // 80 - 25 - 2
STANDOFF_L2_L3    = 53;       // 135 - 80 - 2
STANDOFF_L3_TOP   = 30;       // To top

// === Colors ===
C_ALUMINUM    = [0.78, 0.78, 0.80];
C_PCB         = [0.08, 0.32, 0.08];
C_SLOT        = [0.3, 0.3, 0.35];

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Standoff corner positions (90mm pitch)
function standoff_positions() = [
    [-STANDOFF_PITCH/2, -STANDOFF_PITCH/2],
    [STANDOFF_PITCH/2, -STANDOFF_PITCH/2],
    [STANDOFF_PITCH/2, STANDOFF_PITCH/2],
    [-STANDOFF_PITCH/2, STANDOFF_PITCH/2]
];

// ============================================================================
// MODULE: Universal Slot Grid
// ============================================================================

module slot(length, width) {
    hull() {
        translate([-(length/2 - width/2), 0, 0])
        circle(d=width);
        translate([(length/2 - width/2), 0, 0])
        circle(d=width);
    }
}

module slot_grid_2d(plate_w, plate_d, slot_w=SLOT_WIDTH, slot_l=SLOT_LENGTH,
                     spacing_x=SLOT_SPACING_X, spacing_y=SLOT_SPACING_Y,
                     exclude_center=false, exclude_rect=[0,0]) {
    // Calculate number of slots in each direction
    margin = 15;  // Edge margin
    usable_w = plate_w - margin * 2;
    usable_d = plate_d - margin * 2;

    cols = floor(usable_w / spacing_x);
    rows = floor(usable_d / spacing_y);

    // Center the grid
    start_x = -(cols - 1) * spacing_x / 2;
    start_y = -(rows - 1) * spacing_y / 2;

    for (i = [0:cols-1]) {
        for (j = [0:rows-1]) {
            x = start_x + i * spacing_x;
            y = start_y + j * spacing_y;

            // Skip if in excluded center area
            skip = exclude_center &&
                   abs(x) < exclude_rect[0]/2 &&
                   abs(y) < exclude_rect[1]/2;

            if (!skip) {
                translate([x, y])
                rotate([0, 0, (i + j) % 2 == 0 ? 0 : 90])  // Alternating orientation
                slot(slot_l, slot_w);
            }
        }
    }
}

// ============================================================================
// MODULE: L-shaped Slot (for position adjustment)
// ============================================================================

module l_slot_2d(long_arm=L_SLOT_LENGTH, short_arm=L_SLOT_SHORT, width=L_SLOT_WIDTH) {
    // L-shaped slot: long arm + short arm at 90 degrees
    union() {
        // Long arm (horizontal)
        hull() {
            circle(d=width);
            translate([long_arm, 0]) circle(d=width);
        }
        // Short arm (vertical)
        hull() {
            circle(d=width);
            translate([0, short_arm]) circle(d=width);
        }
    }
}

// ============================================================================
// MODULE: Level 3 Plate - ESP32-P4 EV Board (with L-slots)
// ============================================================================

module plate_level3_2d() {
    difference() {
        // Plate outline with rounded corners
        offset(r=4)
        offset(r=-4)
        square([PLATE_L3_W, PLATE_L3_D], center=true);

        // Standoff mounting holes
        for (pos = standoff_positions()) {
            translate(pos)
            circle(d=STANDOFF_ID + 0.5);
        }

        // L-shaped slots for EV Board corners (position adjustable)
        // Calculate EV Board centered position
        ev_offset_x = 0;  // Centered
        ev_offset_y = 0;  // Centered

        // Top-left L-slot
        translate([ev_offset_x - EV_BOARD_W/2 + 3, ev_offset_y + EV_BOARD_D/2 - 3])
        rotate([0, 0, 180])
        l_slot_2d();

        // Top-right L-slot
        translate([ev_offset_x + EV_BOARD_W/2 - 3, ev_offset_y + EV_BOARD_D/2 - 3])
        rotate([0, 0, 270])
        l_slot_2d();

        // Bottom-right L-slot
        translate([ev_offset_x + EV_BOARD_W/2 - 3, ev_offset_y - EV_BOARD_D/2 + 3])
        rotate([0, 0, 0])
        l_slot_2d();

        // Bottom-left L-slot
        translate([ev_offset_x - EV_BOARD_W/2 + 3, ev_offset_y - EV_BOARD_D/2 + 3])
        rotate([0, 0, 90])
        l_slot_2d();

        // Central cable routing hole
        translate([0, -15])
        offset(r=3)
        offset(r=-3)
        square([CABLE_HOLE_W, CABLE_HOLE_D], center=true);
    }
}

// ============================================================================
// MODULE: Level 2 Plate - DAC + Amp (Universal Slot Grid)
// ============================================================================

module plate_level2_2d() {
    difference() {
        // Plate outline with rounded corners
        offset(r=4)
        offset(r=-4)
        square([PLATE_L2_W, PLATE_L2_D], center=true);

        // Standoff mounting holes
        for (pos = standoff_positions()) {
            translate(pos)
            circle(d=STANDOFF_ID + 0.5);
        }

        // Universal slot grid (full coverage)
        slot_grid_2d(PLATE_L2_W, PLATE_L2_D,
                     exclude_center=false);
    }
}

// ============================================================================
// MODULE: Level 1 Plate - Power + Sensors (with Airflow Opening)
// ============================================================================

module plate_level1_2d() {
    difference() {
        // Plate outline with rounded corners
        offset(r=4)
        offset(r=-4)
        square([PLATE_L1_W, PLATE_L1_D], center=true);

        // Standoff mounting holes
        for (pos = standoff_positions()) {
            translate(pos)
            circle(d=STANDOFF_ID + 0.5);
        }

        // Universal slot grid (excluding center airflow area)
        slot_grid_2d(PLATE_L1_W, PLATE_L1_D,
                     exclude_center=true,
                     exclude_rect=[AIRFLOW_W + 10, AIRFLOW_D + 10]);

        // Large central airflow opening (above bottom vents)
        offset(r=5)
        offset(r=-5)
        square([AIRFLOW_W, AIRFLOW_D], center=true);
    }
}

// ============================================================================
// MODULE: Top Plate - ReSpeaker Mount + LED Diffuser Recess
// ============================================================================

module plate_top_2d() {
    difference() {
        // Circular top plate (slightly larger than ReSpeaker)
        circle(d=RESPEAKER_DIA + 30);

        // ReSpeaker mounting hole (center)
        circle(d=RESPEAKER_DIA);
    }
}

module plate_top_diffuser_recess_2d() {
    // Outer profile of the diffuser recess
    difference() {
        circle(d=LED_RING_OD + 4);  // Recess outer
        circle(d=LED_RING_ID - 4);   // Keep center open
    }
}

// ============================================================================
// 3D PLATE MODULES (for visualization)
// ============================================================================

module plate_level3_3d() {
    color(C_ALUMINUM)
    linear_extrude(height=PLATE_THICKNESS)
    plate_level3_2d();
}

module plate_level2_3d() {
    color(C_ALUMINUM)
    linear_extrude(height=PLATE_THICKNESS)
    plate_level2_2d();
}

module plate_level1_3d() {
    color(C_ALUMINUM)
    linear_extrude(height=PLATE_THICKNESS)
    plate_level1_2d();
}

// ============================================================================
// MODULE: Standoff
// ============================================================================

module standoff(height) {
    color(C_ALUMINUM)
    difference() {
        cylinder(d=STANDOFF_OD, h=height, $fn=6);
        translate([0, 0, -0.1])
        cylinder(d=STANDOFF_ID, h=height + 0.2);
    }
}

// ============================================================================
// MODULE: Complete Tower Assembly
// ============================================================================

module center_tower() {
    translate([0, 0, 0]) {

        // === LEVEL 1: Power + Sensors ===
        translate([0, 0, LEVEL_1_Z]) {
            plate_level1_3d();

            for (pos = standoff_positions()) {
                translate([pos[0], pos[1], PLATE_THICKNESS])
                standoff(STANDOFF_L1_L2);
            }

            // Sensor PCBs visualization
            color(C_PCB, 0.8)
            translate([0, 0, 5])
            cube([60, 45, 3], center=true);
        }

        // === LEVEL 2: DAC + Amp ===
        translate([0, 0, LEVEL_2_Z]) {
            plate_level2_3d();

            for (pos = standoff_positions()) {
                translate([pos[0], pos[1], PLATE_THICKNESS])
                standoff(STANDOFF_L2_L3);
            }

            // ES9039Q2M DAC
            color(C_PCB, 0.8)
            translate([0, 10, 5])
            cube([50, 40, 8], center=true);

            // TPA3116D2 Amplifier
            color([0.1, 0.1, 0.12])
            translate([0, -15, 5])
            cube([45, 35, 10], center=true);
        }

        // === LEVEL 3: ESP32-P4 EV Board ===
        translate([0, 0, LEVEL_3_Z]) {
            plate_level3_3d();

            for (pos = standoff_positions()) {
                translate([pos[0], pos[1], PLATE_THICKNESS])
                standoff(STANDOFF_L3_TOP);
            }

            // ESP32-P4 EV Board (74×88mm)
            color(C_PCB, 0.9)
            translate([0, 0, 5])
            cube([EV_BOARD_W, EV_BOARD_D, 4], center=true);

            color([1, 1, 1])
            translate([0, 0, 8])
            linear_extrude(0.5)
            text("ESP32-P4", size=8, halign="center", valign="center");
        }
    }
}

// ============================================================================
// DXF EXPORT MODULES (2D projection for laser cutting)
// ============================================================================

module dxf_level1() {
    plate_level1_2d();
}

module dxf_level2() {
    plate_level2_2d();
}

module dxf_level3() {
    plate_level3_2d();
}

module dxf_all_plates() {
    // All plates arranged for laser cutting on single sheet
    translate([-70, 0]) plate_level1_2d();
    translate([0, 0]) plate_level2_2d();
    translate([75, 0]) plate_level3_2d();
}

// ============================================================================
// RENDER SELECTION
// ============================================================================

// Uncomment one to render:

center_tower();                  // 3D tower assembly view
// dxf_level1();                 // Level 1 plate for DXF export
// dxf_level2();                 // Level 2 plate for DXF export
// dxf_level3();                 // Level 3 plate for DXF export
// dxf_all_plates();             // All plates on single sheet

// ============================================================================
// DESIGN NOTES v4.2
// ============================================================================
/*
===================================================================
v4.2 PARAMETRIC DESIGN - Updated for ESP32-P4 EV Board
===================================================================

CHANGES FROM v4.1:
  Parameter         v4.1       v4.2         Change
  ────────────────────────────────────────────────────────────
  STANDOFF_PITCH    55mm       90mm         +35mm (for 74mm board)
  TOWER_TOP_W       100mm      120mm        +20mm
  PLATE_L3_W        100mm      120mm        +20mm
  PLATE_L3_D        80mm       100mm        +20mm

ESP32-P4 EV BOARD:
  Width:    74mm
  Depth:    88mm
  Mounting: L-shaped slots for position adjustment (±5mm each axis)

LEVEL 3 PLATE (ESP32-P4):
  - 120×100mm aluminum plate
  - 4× L-shaped slots at corners for adjustable mounting
  - 40×20mm central cable routing hole
  - 90mm standoff pitch (fits 74mm board with margin)

LEVEL 2 PLATE (DAC + Amp):
  - 110×95mm aluminum plate
  - Universal slot grid (4mm × 15mm slots)
  - Grid spacing: 20mm × 18mm
  - Compatible with any TPA3116D2 and ES9039Q2M layout

LEVEL 1 PLATE (Power + Sensors):
  - 100×85mm aluminum plate
  - Universal slot grid (same as Level 2)
  - 50×40mm central airflow opening
  - Positioned above bottom intake vents

TOP PLATE (ReSpeaker + LED):
  - ReSpeaker USB Mic Array mounting
  - LED diffuser ring recess (OD=68mm, ID=50mm, depth=3mm)
  - Nest Mini style lighting effect

SLOT GRID SPECIFICATIONS:
  Slot width:  4mm
  Slot length: 15mm
  X spacing:   20mm (center-to-center)
  Y spacing:   18mm (center-to-center)
  Orientation: Alternating 0°/90° for strength

DXF EXPORT:
  To export DXF files for laser cutting:
  1. Uncomment desired module (dxf_level1, dxf_level2, etc.)
  2. Render (F6)
  3. File → Export → Export as DXF

===================================================================
*/
