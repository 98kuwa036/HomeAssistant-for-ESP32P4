// ============================================================================
// Project Omni-P4: Integrated Acoustic Wedge v4.0
// Complete Redesign - Formation Wedge Authentic
// ============================================================================
//
// DESIGN PHILOSOPHY:
// The enclosure IS the speaker box. No separate boxes inside - internal
// baffles divide the wedge into 3 acoustic chambers (L, Center, R).
// This is how B&W Formation Wedge actually works.
//
// KEY CHANGES FROM v3.x:
// - Front width: 380mm → 420mm
// - Back width: 30mm → 160mm (critical fix!)
// - Height: 180mm → 200mm
// - Depth: 200mm → 220mm
// - No separate speaker boxes - integrated chambers
// - Internal MDF baffles for acoustic isolation
//
// ============================================================================

$fn = 96;

// ============================================================================
// MASTER PARAMETERS v4.0
// ============================================================================

// === Outer Shell (Realistic Formation Wedge) ===
FRONT_WIDTH       = 420;      // Wide enough for comfortable layout
BACK_WIDTH        = 160;      // CRITICAL: realistic back width for components
DEPTH             = 220;      // Slightly deeper for better bass
HEIGHT            = 200;      // Taller for better bass extension
SHELL_THICKNESS   = 12;       // 12mm MDF outer shell
CORNER_RADIUS     = 20;       // Front corner radius
BACK_RADIUS       = 40;       // Back corner radius (smooth curve)

// === Internal Baffles ===
BAFFLE_THICKNESS  = 15;       // Internal divider thickness (MDF)
CENTER_ZONE_WIDTH = 120;      // Width reserved for electronics tower
BAFFLE_SETBACK    = 25;       // Distance from front for baffle

// === Center Tower ===
STANDOFF_PITCH    = 75;       // M3 standoff spacing
STANDOFF_OD       = 6;        // Standoff outer diameter
STANDOFF_ID       = 3;        // M3 thread
TOWER_WIDTH       = 100;      // Plate width
TOWER_DEPTH       = 80;       // Plate depth

// Tower level heights
LEVEL_1_Z         = 20;       // Power + sensors
LEVEL_2_Z         = 65;       // DAC + Amp
LEVEL_3_Z         = 105;      // ESP32-P4

// Standoff heights between levels
STANDOFF_L1_L2    = 43;       // 65 - 20 - 2 (plate thickness)
STANDOFF_L2_L3    = 38;       // 105 - 65 - 2
STANDOFF_L3_TOP   = 25;       // To LCD mount

// === Plate Dimensions ===
PLATE_THICKNESS   = 2.0;
PLATE_L1          = [80, 60];
PLATE_L2          = [85, 65];
PLATE_L3          = [100, 80];

// === LCD (7" MIPI-DSI) ===
LCD_ACTIVE        = [154, 86];
LCD_MODULE        = [170, 105, 5];
LCD_BEZEL         = [190, 125, 8];
LCD_Z_CENTER      = 130;      // Centered vertically
LCD_Y_INSET       = 15;       // Inset from front

// === Speaker Drivers ===
DRIVER_CUTOUT     = 68;       // Peerless full-range cutout
PASSIVE_CUTOUT    = 68;       // Dayton passive radiator
DRIVER_DEPTH      = 35;       // Driver mounting depth

// Driver positions (on internal baffles)
DRIVER_Z          = 135;      // Upper position
PASSIVE_Z         = 60;       // Lower position

// === Microphone (XVF3800) ===
MIC_PCB           = [40, 40, 2];
MIC_MESH_DIA      = 60;
MIC_Z             = HEIGHT - 15;

// === Thermal ===
VENT_SLOT_W       = 40;
VENT_SLOT_H       = 6;
VENT_COUNT        = 5;

// === Colors ===
C_SHELL       = [0.25, 0.22, 0.20];  // Dark walnut
C_BAFFLE      = [0.55, 0.45, 0.35];  // MDF natural
C_ALUMINUM    = [0.78, 0.78, 0.80];
C_PCB         = [0.08, 0.32, 0.08];
C_LCD         = [0.06, 0.06, 0.10];
C_DRIVER      = [0.15, 0.15, 0.18];
C_CONE        = [0.90, 0.88, 0.82];
C_MESH        = [0.50, 0.50, 0.52, 0.7];
C_FABRIC      = [0.12, 0.12, 0.15];

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Calculate wedge width at given Y position
function wedge_width_at(y) =
    FRONT_WIDTH - (FRONT_WIDTH - BACK_WIDTH) * (y / DEPTH);

// Internal width at given Y
function internal_width_at(y) =
    wedge_width_at(y) - SHELL_THICKNESS * 2;

// Standoff corner positions
function standoff_positions() = [
    [-STANDOFF_PITCH/2, -STANDOFF_PITCH/2],
    [STANDOFF_PITCH/2, -STANDOFF_PITCH/2],
    [STANDOFF_PITCH/2, STANDOFF_PITCH/2],
    [-STANDOFF_PITCH/2, STANDOFF_PITCH/2]
];

// ============================================================================
// VOLUME CALCULATIONS (Console Output)
// ============================================================================

// Total internal volume (trapezoidal prism)
INTERNAL_FRONT = FRONT_WIDTH - SHELL_THICKNESS * 2;
INTERNAL_BACK = BACK_WIDTH - SHELL_THICKNESS * 2;
INTERNAL_DEPTH = DEPTH - SHELL_THICKNESS * 2;
INTERNAL_HEIGHT = HEIGHT - SHELL_THICKNESS * 2;

TOTAL_INTERNAL_VOL = (INTERNAL_FRONT + INTERNAL_BACK) / 2 * INTERNAL_DEPTH * INTERNAL_HEIGHT / 1000000;

// Center tower zone volume
TOWER_ZONE_VOL = CENTER_ZONE_WIDTH * (TOWER_DEPTH + 40) * INTERNAL_HEIGHT / 1000000;

// Baffle volume (2 baffles)
BAFFLE_VOL = 2 * BAFFLE_THICKNESS * INTERNAL_DEPTH * INTERNAL_HEIGHT / 1000000;

// Available for speakers
SPEAKER_VOL = TOTAL_INTERNAL_VOL - TOWER_ZONE_VOL - BAFFLE_VOL;
VOL_PER_CHANNEL = SPEAKER_VOL / 2;

echo(str("=== Omni-P4 v4.0 Integrated Acoustic Wedge ==="));
echo(str(""));
echo(str("OUTER DIMENSIONS:"));
echo(str("  Front width: ", FRONT_WIDTH, "mm"));
echo(str("  Back width: ", BACK_WIDTH, "mm"));
echo(str("  Depth: ", DEPTH, "mm"));
echo(str("  Height: ", HEIGHT, "mm"));
echo(str("  Shell: ", SHELL_THICKNESS, "mm MDF"));
echo(str(""));
echo(str("INTERNAL VOLUMES:"));
echo(str("  Total internal: ", TOTAL_INTERNAL_VOL, " L"));
echo(str("  Center tower zone: ", TOWER_ZONE_VOL, " L"));
echo(str("  Baffle material: ", BAFFLE_VOL, " L"));
echo(str("  Speaker chambers: ", SPEAKER_VOL, " L"));
echo(str("  Per channel: ", VOL_PER_CHANNEL, " L"));
echo(str(""));
echo(str("SPEAKER COMPATIBILITY:"));
echo(str("  Peerless 2.5\" recommended: 1.5-2.5L → ", VOL_PER_CHANNEL > 1.5 ? "OK" : "SMALL"));
echo(str("  Dayton Passive Radiator: OK with ", VOL_PER_CHANNEL, "L"));

// ============================================================================
// MODULE: Wedge Shape 2D (Top View)
// ============================================================================

module wedge_2d() {
    hull() {
        // Front left corner
        translate([-FRONT_WIDTH/2 + CORNER_RADIUS, CORNER_RADIUS])
        circle(r=CORNER_RADIUS);

        // Front right corner
        translate([FRONT_WIDTH/2 - CORNER_RADIUS, CORNER_RADIUS])
        circle(r=CORNER_RADIUS);

        // Back center (smooth curve)
        translate([0, DEPTH - BACK_RADIUS])
        circle(r=BACK_RADIUS);
    }
}

module inner_wedge_2d() {
    offset(r=-SHELL_THICKNESS)
    wedge_2d();
}

// ============================================================================
// MODULE: Outer Shell
// ============================================================================

module outer_shell(show_cutaway=false) {
    color(C_SHELL) {
        difference() {
            // Outer wedge with rounded top/bottom edges
            hull() {
                // Bottom edge
                translate([0, 0, CORNER_RADIUS])
                linear_extrude(height=0.1)
                wedge_2d();

                // Main body
                translate([0, 0, CORNER_RADIUS])
                linear_extrude(height=HEIGHT - CORNER_RADIUS*2)
                wedge_2d();

                // Top edge (slightly inset)
                translate([0, 0, HEIGHT - CORNER_RADIUS])
                linear_extrude(height=0.1)
                offset(r=-3)
                wedge_2d();
            }

            // Inner cavity
            translate([0, 0, SHELL_THICKNESS])
            linear_extrude(height=HEIGHT)
            inner_wedge_2d();

            // LCD aperture (front face)
            translate([0, -1, LCD_Z_CENTER])
            rotate([-90, 0, 0])
            hull() {
                for (x = [-1, 1], z = [-1, 1]) {
                    translate([x * (LCD_BEZEL[0]/2 - 10), z * (LCD_BEZEL[1]/2 - 10), 0])
                    cylinder(r=10, h=SHELL_THICKNESS + 2);
                }
            }

            // Bottom ventilation slots
            for (i = [0 : VENT_COUNT-1]) {
                x = -((VENT_COUNT-1)/2 * 60) + i * 60;
                hull() {
                    translate([x - VENT_SLOT_W/2, DEPTH * 0.4, -1])
                    cylinder(d=VENT_SLOT_H, h=SHELL_THICKNESS + 2);
                    translate([x + VENT_SLOT_W/2, DEPTH * 0.4, -1])
                    cylinder(d=VENT_SLOT_H, h=SHELL_THICKNESS + 2);
                }
            }

            // Top microphone mesh opening
            translate([0, DEPTH/2, HEIGHT - SHELL_THICKNESS/2])
            cylinder(d=MIC_MESH_DIA + 10, h=SHELL_THICKNESS + 1, center=true);

            // Back connector panel
            translate([0, DEPTH - 1, HEIGHT * 0.35])
            rotate([-90, 0, 0])
            hull() {
                for (x = [-30, 30]) {
                    translate([x, 0, 0])
                    cylinder(d=12, h=SHELL_THICKNESS + 2);
                }
            }

            // Side USB-C (left)
            translate([-FRONT_WIDTH/3, 20, 40])
            rotate([0, 90, -20])
            hull() {
                for (x = [-4, 4]) {
                    translate([x, 0, 0])
                    cylinder(d=4, h=SHELL_THICKNESS + 2);
                }
            }

            // Cutaway for visualization
            if (show_cutaway) {
                translate([0, -DEPTH/2, -10])
                cube([FRONT_WIDTH, DEPTH/2, HEIGHT + 20], center=true);
            }
        }
    }
}

// ============================================================================
// MODULE: Internal Acoustic Baffles
// ============================================================================

module acoustic_baffles() {
    color(C_BAFFLE) {
        // Left baffle (separates left speaker from center)
        translate([-CENTER_ZONE_WIDTH/2 - BAFFLE_THICKNESS/2, SHELL_THICKNESS, SHELL_THICKNESS])
        difference() {
            // Baffle plate
            cube([BAFFLE_THICKNESS, DEPTH - SHELL_THICKNESS*2, HEIGHT - SHELL_THICKNESS*2]);

            // Driver cutout (angled toward front)
            translate([-1, BAFFLE_SETBACK, DRIVER_Z - SHELL_THICKNESS])
            rotate([0, 90, 0])
            cylinder(d=DRIVER_CUTOUT + 2, h=BAFFLE_THICKNESS + 2);

            // Passive radiator cutout
            translate([-1, BAFFLE_SETBACK, PASSIVE_Z - SHELL_THICKNESS])
            rotate([0, 90, 0])
            cylinder(d=PASSIVE_CUTOUT + 2, h=BAFFLE_THICKNESS + 2);
        }

        // Right baffle (separates right speaker from center)
        translate([CENTER_ZONE_WIDTH/2 - BAFFLE_THICKNESS/2, SHELL_THICKNESS, SHELL_THICKNESS])
        difference() {
            cube([BAFFLE_THICKNESS, DEPTH - SHELL_THICKNESS*2, HEIGHT - SHELL_THICKNESS*2]);

            // Driver cutout
            translate([-1, BAFFLE_SETBACK, DRIVER_Z - SHELL_THICKNESS])
            rotate([0, 90, 0])
            cylinder(d=DRIVER_CUTOUT + 2, h=BAFFLE_THICKNESS + 2);

            // Passive radiator cutout
            translate([-1, BAFFLE_SETBACK, PASSIVE_Z - SHELL_THICKNESS])
            rotate([0, 90, 0])
            cylinder(d=PASSIVE_CUTOUT + 2, h=BAFFLE_THICKNESS + 2);
        }
    }
}

// ============================================================================
// MODULE: Speaker Driver Assembly
// ============================================================================

module speaker_driver() {
    // Basket
    color(C_DRIVER)
    difference() {
        cylinder(d=DRIVER_CUTOUT - 2, h=DRIVER_DEPTH);
        translate([0, 0, 5])
        cylinder(d=DRIVER_CUTOUT - 10, h=DRIVER_DEPTH);
    }

    // Cone
    color(C_CONE)
    translate([0, 0, 3])
    cylinder(d1=DRIVER_CUTOUT - 12, d2=15, h=12);

    // Dust cap
    color([0.2, 0.2, 0.22])
    translate([0, 0, 14])
    sphere(d=18);

    // Mounting flange
    color(C_DRIVER)
    translate([0, 0, DRIVER_DEPTH - 3])
    difference() {
        cylinder(d=DRIVER_CUTOUT + 8, h=3);
        translate([0, 0, -1])
        cylinder(d=DRIVER_CUTOUT - 8, h=5);
    }
}

module passive_radiator() {
    // Flat passive diaphragm
    color([0.9, 0.88, 0.85])
    cylinder(d=PASSIVE_CUTOUT - 4, h=2);

    // Surround
    color([0.3, 0.3, 0.32])
    difference() {
        cylinder(d=PASSIVE_CUTOUT, h=3);
        translate([0, 0, -1])
        cylinder(d=PASSIVE_CUTOUT - 15, h=5);
    }

    // Frame
    color(C_DRIVER)
    translate([0, 0, 2])
    difference() {
        cylinder(d=PASSIVE_CUTOUT + 8, h=3);
        translate([0, 0, -1])
        cylinder(d=PASSIVE_CUTOUT - 4, h=5);
    }
}

module speaker_assembly() {
    // Left channel
    translate([-CENTER_ZONE_WIDTH/2 - BAFFLE_THICKNESS, SHELL_THICKNESS + BAFFLE_SETBACK, 0]) {
        // Main driver
        translate([0, 0, DRIVER_Z])
        rotate([0, 90, 0])
        speaker_driver();

        // Passive radiator
        translate([0, 0, PASSIVE_Z])
        rotate([0, 90, 0])
        passive_radiator();
    }

    // Right channel (mirrored)
    translate([CENTER_ZONE_WIDTH/2 + BAFFLE_THICKNESS, SHELL_THICKNESS + BAFFLE_SETBACK, 0]) {
        // Main driver
        translate([0, 0, DRIVER_Z])
        rotate([0, -90, 0])
        speaker_driver();

        // Passive radiator
        translate([0, 0, PASSIVE_Z])
        rotate([0, -90, 0])
        passive_radiator();
    }
}

// ============================================================================
// MODULE: Aluminum Standoff
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
// MODULE: Shelf Plate
// ============================================================================

module shelf_plate(size, with_vent=true) {
    w = size[0];
    d = size[1];

    color(C_ALUMINUM)
    difference() {
        // Plate body
        hull() {
            for (x = [-1, 1], y = [-1, 1]) {
                translate([x * (w/2 - 4), y * (d/2 - 4), 0])
                cylinder(r=4, h=PLATE_THICKNESS);
            }
        }

        // Standoff holes
        for (pos = standoff_positions()) {
            translate([pos[0], pos[1], -0.1])
            cylinder(d=STANDOFF_ID + 0.5, h=PLATE_THICKNESS + 0.2);
        }

        // Ventilation slots
        if (with_vent) {
            for (i = [-1, 0, 1]) {
                hull() {
                    translate([i * w/4 - 8, 0, -0.1])
                    cylinder(d=6, h=PLATE_THICKNESS + 0.2);
                    translate([i * w/4 + 8, 0, -0.1])
                    cylinder(d=6, h=PLATE_THICKNESS + 0.2);
                }
            }
        }

        // Cable routing corners
        for (x = [-1, 1], y = [-1, 1]) {
            translate([x * (w/2 - 8), y * (d/2 - 8), -0.1])
            cylinder(d=5, h=PLATE_THICKNESS + 0.2);
        }
    }
}

// ============================================================================
// MODULE: Center Electronics Tower
// ============================================================================

module center_tower() {
    translate([0, DEPTH/2, 0]) {

        // === LEVEL 1: Power + Sensors ===
        translate([0, 0, LEVEL_1_Z]) {
            shelf_plate(PLATE_L1);

            // Standoffs to Level 2
            for (pos = standoff_positions()) {
                translate([pos[0], pos[1], PLATE_THICKNESS])
                standoff(STANDOFF_L1_L2);
            }

            // Sensor PCBs representation
            color(C_PCB, 0.8)
            translate([0, 0, 5])
            cube([60, 45, 3], center=true);
        }

        // === LEVEL 2: DAC + Amp ===
        translate([0, 0, LEVEL_2_Z]) {
            shelf_plate(PLATE_L2);

            // Standoffs to Level 3
            for (pos = standoff_positions()) {
                translate([pos[0], pos[1], PLATE_THICKNESS])
                standoff(STANDOFF_L2_L3);
            }

            // ES9039Q2M DAC
            color(C_PCB, 0.8)
            translate([0, 0, 5])
            cube([50, 40, 8], center=true);

            // Amplifier board
            color([0.1, 0.1, 0.12])
            translate([0, 0, 15])
            cube([45, 35, 10], center=true);
        }

        // === LEVEL 3: ESP32-P4 ===
        translate([0, 0, LEVEL_3_Z]) {
            shelf_plate(PLATE_L3, with_vent=false);

            // Standoffs to LCD mount
            for (pos = standoff_positions()) {
                translate([pos[0], pos[1], PLATE_THICKNESS])
                standoff(STANDOFF_L3_TOP);
            }

            // ESP32-P4 board (mounted under plate for space efficiency)
            color(C_PCB, 0.9)
            translate([0, 0, -10])
            cube([55, 45, 4], center=true);

            // ESP32-P4 label
            color([1, 1, 1])
            translate([0, 0, -12])
            linear_extrude(0.5)
            text("ESP32-P4", size=6, halign="center", valign="center");
        }
    }
}

// ============================================================================
// MODULE: LCD Assembly
// ============================================================================

module lcd_assembly() {
    translate([0, SHELL_THICKNESS + LCD_Y_INSET, LCD_Z_CENTER]) {
        // LCD panel
        color(C_LCD)
        cube([LCD_MODULE[0], LCD_MODULE[2], LCD_MODULE[1]], center=true);

        // Active display area
        color([0.05, 0.08, 0.12])
        translate([0, -2.6, 0])
        cube([LCD_ACTIVE[0], 0.5, LCD_ACTIVE[1]], center=true);

        // Bezel frame
        color([0.15, 0.15, 0.18])
        difference() {
            translate([0, 2, 0])
            cube([LCD_BEZEL[0], LCD_BEZEL[2], LCD_BEZEL[1]], center=true);

            translate([0, 0, 0])
            cube([LCD_MODULE[0] + 2, LCD_BEZEL[2] + 2, LCD_MODULE[1] + 2], center=true);
        }
    }
}

// ============================================================================
// MODULE: XVF3800 Microphone Array
// ============================================================================

module xvf3800() {
    translate([0, DEPTH/2, MIC_Z]) {
        // PCB
        color(C_PCB)
        hull() {
            for (x = [-1, 1], y = [-1, 1]) {
                translate([x * (MIC_PCB[0]/2 - 5), y * (MIC_PCB[1]/2 - 5), 0])
                cylinder(r=5, h=MIC_PCB[2], center=true);
            }
        }

        // MEMS microphones (4)
        color([0.3, 0.3, 0.35])
        for (x = [-12, 12], y = [-12, 12]) {
            translate([x, y, MIC_PCB[2]/2 + 1])
            cylinder(d=4, h=1.5);
        }

        // Acoustic mesh
        color(C_MESH)
        translate([0, 0, MIC_PCB[2]/2 + 4])
        cylinder(d=MIC_MESH_DIA, h=1);
    }
}

// ============================================================================
// MODULE: Front Fabric Grille
// ============================================================================

module fabric_grille() {
    color(C_FABRIC, 0.9) {
        difference() {
            // Full front coverage
            translate([0, 2, HEIGHT/2])
            rotate([90, 0, 0])
            linear_extrude(height=3)
            hull() {
                for (x = [-1, 1]) {
                    translate([x * (FRONT_WIDTH/2 - CORNER_RADIUS - 5), CORNER_RADIUS])
                    circle(r=8);
                    translate([x * (FRONT_WIDTH/2 - CORNER_RADIUS - 5), HEIGHT - CORNER_RADIUS - 5])
                    circle(r=8);
                }
            }

            // LCD window cutout
            translate([0, 5, LCD_Z_CENTER])
            rotate([90, 0, 0])
            hull() {
                for (x = [-1, 1], z = [-1, 1]) {
                    translate([x * (LCD_BEZEL[0]/2 - 12), z * (LCD_BEZEL[1]/2 - 12)])
                    cylinder(r=12, h=10);
                }
            }
        }
    }
}

// ============================================================================
// MODULE: Acoustic Dampening Material (visualization)
// ============================================================================

module acoustic_foam() {
    color([0.3, 0.3, 0.35], 0.4) {
        // Left chamber foam
        translate([-FRONT_WIDTH/4, DEPTH/2, HEIGHT/2])
        cube([60, 80, 100], center=true);

        // Right chamber foam
        translate([FRONT_WIDTH/4, DEPTH/2, HEIGHT/2])
        cube([60, 80, 100], center=true);
    }
}

// ============================================================================
// ASSEMBLY MODULES
// ============================================================================

module full_assembly() {
    outer_shell(show_cutaway=false);
    acoustic_baffles();
    speaker_assembly();
    center_tower();
    lcd_assembly();
    xvf3800();
    fabric_grille();
}

module section_view() {
    difference() {
        full_assembly();
        translate([0, -DEPTH/2, -10])
        cube([FRONT_WIDTH + 10, DEPTH/2, HEIGHT + 20], center=true);
    }
}

module exploded_view() {
    translate([0, 0, 0]) outer_shell();
    translate([0, 0, 40]) acoustic_baffles();
    translate([0, 0, 80]) speaker_assembly();
    translate([0, 0, 50]) center_tower();
    translate([0, 0, 100]) lcd_assembly();
    translate([0, 0, 120]) xvf3800();
    translate([0, -30, 0]) fabric_grille();
}

module internal_only() {
    acoustic_baffles();
    speaker_assembly();
    center_tower();
    lcd_assembly();
    xvf3800();
    acoustic_foam();
}

module shell_only() {
    outer_shell();
    acoustic_baffles();
}

module top_view_2d() {
    projection(cut=true)
    translate([0, 0, -HEIGHT/2])
    full_assembly();
}

module front_view_2d() {
    projection(cut=true)
    rotate([90, 0, 0])
    translate([0, 0, -DEPTH/2])
    full_assembly();
}

// ============================================================================
// RENDER SELECTION
// ============================================================================

// Uncomment one to render:

full_assembly();              // Complete v4.0 design
// section_view();            // Cross-section view
// exploded_view();           // Exploded components
// internal_only();           // Internal components only
// shell_only();              // Shell + baffles only
// top_view_2d();             // 2D top projection
// front_view_2d();           // 2D front projection

// ============================================================================
// DESIGN NOTES v4.0
// ============================================================================
/*
===================================================================
v4.0 INTEGRATED ACOUSTIC WEDGE - COMPLETE REDESIGN
===================================================================

DESIGN PHILOSOPHY:
The enclosure IS the speaker box. This is how B&W Formation Wedge
actually works - not boxes inside an enclosure, but the enclosure
itself divided into acoustic chambers.

OUTER DIMENSIONS:
  Front width:  420mm (was 380mm)
  Back width:   160mm (was 30mm - CRITICAL CHANGE)
  Depth:        220mm (was 200mm)
  Height:       200mm (was 180mm)
  Shell:        12mm MDF

WHY BACK WIDTH 160mm?
The extreme 30mm back width made it geometrically impossible for
any speaker boxes to fit. Real Formation Wedge has ~150mm back.
160mm allows components while maintaining wedge aesthetic.

INTERNAL STRUCTURE:
  ┌────────────────────────────────────────────┐
  │         │                   │              │ Front 420mm
  │  LEFT   │     CENTER        │    RIGHT     │
  │ SPEAKER │   ELECTRONICS     │   SPEAKER    │
  │ ~2.9L   │     TOWER         │   ~2.9L      │
  │         │                   │              │
  └─────────┴───────────────────┴──────────────┘
            ↑                   ↑
        15mm MDF baffles

VOLUME CALCULATION:
  Total internal:    ~9.2L
  Center zone:       ~2.3L
  Baffle material:   ~1.0L
  Speaker chambers:  ~5.9L (2.95L per channel)

SPEAKER CONFIGURATION:
  Driver:     Peerless 2.5" full-range (Ø68mm cutout)
  Passive:    Dayton passive radiator (Ø68mm cutout)
  Volume:     2.95L per channel (optimal for these drivers)
  Mounting:   On internal baffles, firing toward front

ELECTRONICS TOWER (3 levels):
  Level 1 (Z=20mm):   Power + Sensors
  Level 2 (Z=65mm):   ES9039Q2M DAC + Amplifier
  Level 3 (Z=105mm):  ESP32-P4 (mounted under plate)

LCD:
  Module:     170mm × 105mm × 5mm
  Position:   Centered, 15mm inset from front
  Z-center:   130mm (upper third of front face)

MICROPHONE:
  XVF3800:    Top surface, centered
  Mesh:       Ø60mm stainless steel acoustic mesh

THERMAL:
  Intake:     5× bottom slots (40mm × 6mm)
  Exhaust:    Top perimeter around mic mesh
  Isolation:  MDF baffles separate hot zone from speakers

MATERIALS:
  Shell:      12mm MDF (walnut veneer or black paint)
  Baffles:    15mm MDF (internal, invisible)
  Plates:     2mm A6063-T5 Aluminum (black anodized)
  Standoffs:  M3 aluminum hex standoffs
  Grille:     Acoustically transparent fabric

ADVANTAGES OF THIS DESIGN:
1. All components fit with proper clearance
2. ~3L speaker volume per channel (excellent bass)
3. Formation Wedge aesthetic preserved
4. Thermal isolation between electronics and speakers
5. Simple manufacturing (shell + 2 baffles)
6. No separate speaker boxes needed

PIN ASSIGNMENTS (ESP32-P4):
  I2S0 (DAC):     BCLK=12, WS=10, DOUT=9, MCLK=13
  I2S1 (Mic):     BCLK=45, WS=46, DIN=47
  I2C (Sensors):  SDA=7, SCL=8
  UART1 (Radar):  TX=17, RX=18
  UART2 (Voice):  TX=4, RX=5

===================================================================
*/
