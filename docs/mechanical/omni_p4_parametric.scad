// ============================================================================
// Project Omni-P4: Parametric 3D Model (OpenSCAD)
// Formation Wedge Style Smart Speaker with ESP32-P4
// ============================================================================
// This file serves as a dimensional reference and can be exported to STEP
// for use in Fusion 360, SolidWorks, or other CAD software.
// ============================================================================

// === MAIN PARAMETERS (Adjust these) ===
$fn = 64;  // Resolution for curves

// Overall dimensions
FRONT_WIDTH = 380;       // mm
BACK_WIDTH = 150;        // mm
DEPTH = 200;             // mm
HEIGHT = 180;            // mm
WEDGE_ANGLE = 120;       // degrees

// LCD dimensions
LCD_ACTIVE_W = 154;      // mm
LCD_ACTIVE_H = 86;       // mm
LCD_MODULE_W = 170;      // mm
LCD_MODULE_H = 105;      // mm
LCD_BEZEL_TOP = 10;      // mm
LCD_BEZEL_BOTTOM = 15;   // mm
LCD_BEZEL_SIDE = 18;     // mm

// Center tower
TOWER_BASE_W = 80;       // mm (Level 1)
TOWER_BASE_D = 60;       // mm
TOWER_TOP_W = 100;       // mm (Level 3)
TOWER_TOP_D = 80;        // mm
STANDOFF_PITCH = 75;     // mm (square pattern)
STANDOFF_DIA = 6;        // mm
STANDOFF_H1 = 40;        // mm (L1 to L2)
STANDOFF_H2 = 35;        // mm (L2 to L3)
STANDOFF_H3 = 25;        // mm (L3 to LCD)
SHELF_THICKNESS = 2;     // mm (aluminum)

// Speaker box (each)
SPK_BOX_W = 80;          // mm internal
SPK_BOX_H = 120;         // mm internal
SPK_BOX_D = 180;         // mm internal
MDF_THICKNESS = 19;      // mm
DRIVER_CUTOUT = 65;      // mm diameter
PASSIVE_RAD_CUTOUT = 65; // mm diameter
PASSIVE_RAD_STROKE = 15; // mm clearance

// XVF3800 mic array
MIC_PCB_SIZE = 40;       // mm square
MIC_MESH_DIA = 60;       // mm

// === COLORS FOR VISUALIZATION ===
COLOR_ALUMINUM = [0.7, 0.7, 0.75];
COLOR_MDF = [0.6, 0.45, 0.3];
COLOR_PCB = [0.1, 0.4, 0.1];
COLOR_LCD = [0.1, 0.1, 0.15];
COLOR_FABRIC = [0.2, 0.2, 0.2];
COLOR_RUBBER = [0.15, 0.15, 0.15];

// ============================================================================
// MODULE: Outer Shell (Wedge Shape)
// ============================================================================
module outer_shell() {
    color(COLOR_MDF, 0.3) {
        difference() {
            // Outer wedge
            linear_extrude(height = HEIGHT) {
                polygon([
                    [-FRONT_WIDTH/2, 0],
                    [FRONT_WIDTH/2, 0],
                    [BACK_WIDTH/2, DEPTH],
                    [-BACK_WIDTH/2, DEPTH]
                ]);
            }

            // Inner cavity (6mm wall)
            translate([0, 6, 6])
            linear_extrude(height = HEIGHT - 12) {
                polygon([
                    [-FRONT_WIDTH/2 + 6, 0],
                    [FRONT_WIDTH/2 - 6, 0],
                    [BACK_WIDTH/2 - 6, DEPTH - 6],
                    [-BACK_WIDTH/2 + 6, DEPTH - 6]
                ]);
            }

            // LCD cutout
            translate([0, -1, HEIGHT/2 + 20])
            rotate([-90, 0, 0])
            cube([LCD_MODULE_W + 4, LCD_MODULE_H + 4, 10], center=true);
        }
    }
}

// ============================================================================
// MODULE: Aluminum Shelf Plate
// ============================================================================
module shelf_plate(width, depth, with_cutouts=true) {
    color(COLOR_ALUMINUM) {
        difference() {
            cube([width, depth, SHELF_THICKNESS], center=true);

            if (with_cutouts) {
                // Ventilation slots (40% open)
                for (x = [-width/4, 0, width/4]) {
                    for (y = [-depth/4, depth/4]) {
                        translate([x, y, 0])
                        cube([width/6, depth/6, SHELF_THICKNESS + 1], center=true);
                    }
                }
            }

            // Standoff holes
            for (x = [-STANDOFF_PITCH/2, STANDOFF_PITCH/2]) {
                for (y = [-STANDOFF_PITCH/2, STANDOFF_PITCH/2]) {
                    translate([x, y, 0])
                    cylinder(d=3.2, h=SHELF_THICKNESS + 1, center=true);
                }
            }
        }
    }
}

// ============================================================================
// MODULE: Aluminum Standoff
// ============================================================================
module standoff(height) {
    color(COLOR_ALUMINUM) {
        difference() {
            cylinder(d=STANDOFF_DIA, h=height);
            translate([0, 0, -0.5])
            cylinder(d=2.5, h=height + 1);  // M3 tap hole
        }
    }
}

// ============================================================================
// MODULE: Center Electronics Tower
// ============================================================================
module center_tower() {
    // Level 1 - Base (Power + Sensors)
    translate([0, DEPTH/2, 20]) {
        shelf_plate(TOWER_BASE_W, TOWER_BASE_D);

        // Standoffs to Level 2
        for (x = [-STANDOFF_PITCH/2, STANDOFF_PITCH/2]) {
            for (y = [-STANDOFF_PITCH/2, STANDOFF_PITCH/2]) {
                translate([x, y, SHELF_THICKNESS/2])
                standoff(STANDOFF_H1);
            }
        }
    }

    // Level 2 - Middle (DAC + Amp)
    translate([0, DEPTH/2, 20 + STANDOFF_H1 + SHELF_THICKNESS]) {
        shelf_plate((TOWER_BASE_W + TOWER_TOP_W)/2, (TOWER_BASE_D + TOWER_TOP_D)/2);

        // Mock DAC board
        color(COLOR_PCB, 0.7)
        translate([0, 0, 5])
        cube([70, 50, 2], center=true);

        // Standoffs to Level 3
        for (x = [-STANDOFF_PITCH/2, STANDOFF_PITCH/2]) {
            for (y = [-STANDOFF_PITCH/2, STANDOFF_PITCH/2]) {
                translate([x, y, SHELF_THICKNESS/2])
                standoff(STANDOFF_H2);
            }
        }
    }

    // Level 3 - Top (ESP32-P4)
    translate([0, DEPTH/2, 20 + STANDOFF_H1 + STANDOFF_H2 + SHELF_THICKNESS*2]) {
        shelf_plate(TOWER_TOP_W, TOWER_TOP_D, false);

        // Mock ESP32-P4 board
        color(COLOR_PCB, 0.7)
        translate([0, 0, 5])
        cube([55, 45, 3], center=true);
    }
}

// ============================================================================
// MODULE: Speaker Box (MDF)
// ============================================================================
module speaker_box() {
    color(COLOR_MDF) {
        difference() {
            // Outer box
            cube([
                SPK_BOX_W + MDF_THICKNESS*2,
                SPK_BOX_D + MDF_THICKNESS*2,
                SPK_BOX_H + MDF_THICKNESS*2
            ]);

            // Inner cavity
            translate([MDF_THICKNESS, MDF_THICKNESS, MDF_THICKNESS])
            cube([SPK_BOX_W, SPK_BOX_D, SPK_BOX_H]);

            // Main driver cutout (front)
            translate([
                (SPK_BOX_W + MDF_THICKNESS*2)/2,
                -1,
                SPK_BOX_H/2 + MDF_THICKNESS + 20
            ])
            rotate([-90, 0, 0])
            cylinder(d=DRIVER_CUTOUT, h=MDF_THICKNESS + 2);

            // Passive radiator cutout (front, below driver)
            translate([
                (SPK_BOX_W + MDF_THICKNESS*2)/2,
                -1,
                SPK_BOX_H/2 + MDF_THICKNESS - 30
            ])
            rotate([-90, 0, 0])
            cylinder(d=PASSIVE_RAD_CUTOUT, h=MDF_THICKNESS + 2);
        }
    }

    // Hanenite vibration pad (bottom)
    color(COLOR_RUBBER)
    translate([0, 0, -5])
    cube([SPK_BOX_W + MDF_THICKNESS*2, SPK_BOX_D + MDF_THICKNESS*2, 5]);
}

// ============================================================================
// MODULE: 7-inch LCD
// ============================================================================
module lcd_display() {
    // LCD module
    color(COLOR_LCD) {
        cube([LCD_MODULE_W, 5, LCD_MODULE_H], center=true);
    }

    // Active display area (bright)
    color([0.2, 0.3, 0.4])
    translate([0, -2, 0])
    cube([LCD_ACTIVE_W, 1, LCD_ACTIVE_H], center=true);
}

// ============================================================================
// MODULE: XVF3800 Microphone Array
// ============================================================================
module xvf3800() {
    // PCB
    color(COLOR_PCB)
    cube([MIC_PCB_SIZE, MIC_PCB_SIZE, 2], center=true);

    // MEMS microphones (4x)
    color([0.3, 0.3, 0.35]) {
        for (x = [-12, 12]) {
            for (y = [-12, 12]) {
                translate([x, y, 1.5])
                cube([4, 3, 1.5], center=true);
            }
        }
    }

    // Acoustic mesh cover
    color([0.6, 0.6, 0.6], 0.5)
    translate([0, 0, 5])
    cylinder(d=MIC_MESH_DIA, h=0.5);
}

// ============================================================================
// MAIN ASSEMBLY
// ============================================================================
module assembly() {
    // Outer shell
    outer_shell();

    // Center electronics tower
    center_tower();

    // Left speaker box
    translate([-FRONT_WIDTH/2 + 30, 30, 10])
    rotate([0, 0, -15])
    speaker_box();

    // Right speaker box
    translate([FRONT_WIDTH/2 - 30 - (SPK_BOX_W + MDF_THICKNESS*2), 30, 10])
    rotate([0, 0, 15])
    speaker_box();

    // LCD display
    translate([0, 5, HEIGHT/2 + 20])
    rotate([90, 0, 0])
    lcd_display();

    // XVF3800 on top
    translate([0, DEPTH/2, HEIGHT - 10])
    xvf3800();
}

// Render the assembly
assembly();

// ============================================================================
// EXPORT NOTES:
// 1. Use OpenSCAD to render (F6) and export as STL or CSG
// 2. Import into Fusion 360 or FreeCAD for detailed modeling
// 3. Key dimensions are parameterized at the top of this file
// ============================================================================
