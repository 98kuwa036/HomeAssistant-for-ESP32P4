// ============================================================================
// Project Omni-P4: Formation Wedge Style v3.0
// Slim Design with Trapezoidal Speaker Boxes
// ============================================================================
//
// v3.0 Changes:
// - Trapezoidal speaker boxes following 120° wedge angle
// - Compact tower (55mm pitch, ESP32-P4 under Level 3)
// - LCD inset 10mm for curved bezel integration
// - Rounded corners using hull() + offset() for streamlined look
// - High resolution ($fn=128)
//
// ============================================================================

$fn = 128;  // High resolution for smooth curves

// ============================================================================
// MASTER PARAMETERS
// ============================================================================

// === Outer Shell (Wedge) ===
FRONT_WIDTH     = 380;
BACK_WIDTH      = 150;
DEPTH           = 200;
HEIGHT          = 180;
WALL_THICKNESS  = 6;
WEDGE_ANGLE     = 120;
CORNER_RADIUS   = 10;     // NEW: Corner rounding

// === Compact Center Tower ===
STANDOFF_PITCH  = 55;     // CHANGED: 75mm → 55mm (compact)
STANDOFF_OD     = 5;      // Slightly smaller standoffs
STANDOFF_ID     = 2.5;

// Standoff heights
STANDOFF_L1_L2  = 35;     // Reduced for compact design
STANDOFF_L2_L3  = 30;
STANDOFF_L3_TOP = 20;

// === Shelf Plates (Compact) ===
PLATE_THICKNESS = 2.0;

LEVEL_1_Z = 15;
LEVEL_2_Z = LEVEL_1_Z + STANDOFF_L1_L2 + PLATE_THICKNESS;
LEVEL_3_Z = LEVEL_2_Z + STANDOFF_L2_L3 + PLATE_THICKNESS;

// Compact plate dimensions
PLATE_L1 = [60, 50];      // CHANGED: Smaller
PLATE_L2 = [65, 55];
PLATE_L3 = [70, 60];

CABLE_DUCT_SIZE = [10, 6];

// === LCD (Inset Configuration) ===
LCD_ACTIVE     = [154, 86];
LCD_MODULE     = [170, 105, 5];
LCD_BEZEL      = [190, 130, 4];
LCD_Z_CENTER   = 110;
LCD_Y_INSET    = 10;      // NEW: 10mm inset from front

// === Trapezoidal Speaker Box ===
MDF_THICKNESS   = 19;

// Speaker box follows wedge angle
// Front width > Back width
SPK_FRONT_W     = 95;     // Width at front face
SPK_BACK_W      = 70;     // Width at back (narrower)
SPK_DEPTH       = 170;    // Depth (front to back)
SPK_HEIGHT      = 130;    // Height

// Driver positions
DRIVER_DIA      = 65;
PASSIVE_RAD_DIA = 65;

// Speaker positioning
SPK_X_OFFSET    = 115;    // Distance from center (adjusted for compact tower)
SPK_Y_OFFSET    = 25;     // Closer to front
SPK_Z_OFFSET    = 12;

// === XVF3800 ===
MIC_PCB         = [40, 40, 2];
MIC_MESH_DIA    = 55;     // Slightly smaller
MIC_Z           = HEIGHT - 10;

// === Other ===
HANENITE_THICKNESS = 5;

// === Colors ===
C_ALUMINUM  = [0.78, 0.78, 0.80];
C_MDF       = [0.60, 0.48, 0.35];
C_MDF_DARK  = [0.40, 0.32, 0.22];
C_PCB       = [0.08, 0.32, 0.08];
C_LCD       = [0.06, 0.06, 0.10];
C_RUBBER    = [0.10, 0.10, 0.10];
C_MESH      = [0.55, 0.55, 0.58, 0.6];
C_FRAME     = [0.22, 0.22, 0.25];
C_FABRIC    = [0.15, 0.15, 0.18];

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Wedge polygon (basic)
function wedge_points() = [
    [-FRONT_WIDTH/2, 0],
    [FRONT_WIDTH/2, 0],
    [BACK_WIDTH/2, DEPTH],
    [-BACK_WIDTH/2, DEPTH]
];

// Calculate wedge width at given Y position
function wedge_width_at(y) =
    FRONT_WIDTH - (FRONT_WIDTH - BACK_WIDTH) * (y / DEPTH);

// Standoff positions
function standoff_positions() = [
    [-STANDOFF_PITCH/2, -STANDOFF_PITCH/2],
    [STANDOFF_PITCH/2, -STANDOFF_PITCH/2],
    [STANDOFF_PITCH/2, STANDOFF_PITCH/2],
    [-STANDOFF_PITCH/2, STANDOFF_PITCH/2]
];

// ============================================================================
// MODULE: Rounded Wedge Shell (Formation Wedge Style)
// ============================================================================
module rounded_wedge_2d() {
    offset(r=CORNER_RADIUS)
    offset(r=-CORNER_RADIUS)
    polygon(wedge_points());
}

module outer_shell_rounded(show_cutaway=false) {
    color(C_MDF_DARK, 0.35) {
        difference() {
            // Rounded outer wedge
            hull() {
                // Bottom rounded edge
                translate([0, 0, CORNER_RADIUS])
                linear_extrude(height=0.1)
                rounded_wedge_2d();

                // Main body
                translate([0, 0, CORNER_RADIUS])
                linear_extrude(height=HEIGHT - CORNER_RADIUS*2)
                rounded_wedge_2d();

                // Top rounded edge
                translate([0, 0, HEIGHT - CORNER_RADIUS])
                linear_extrude(height=0.1)
                offset(r=-2)
                rounded_wedge_2d();
            }

            // Inner cavity
            translate([0, WALL_THICKNESS, WALL_THICKNESS])
            linear_extrude(height=HEIGHT - WALL_THICKNESS*2) {
                offset(r=-WALL_THICKNESS)
                rounded_wedge_2d();
            }

            // LCD aperture (front, inset position)
            translate([0, -1, LCD_Z_CENTER])
            rotate([-90, 0, 0])
            hull() {
                for (x = [-1, 1]) {
                    for (z = [-1, 1]) {
                        translate([x * (LCD_BEZEL[0]/2 - 8), z * (LCD_BEZEL[1]/2 - 8), 0])
                        cylinder(r=8, h=WALL_THICKNESS + 2);
                    }
                }
            }

            // Bottom intake vents (streamlined slots)
            for (x = [-100, -50, 0, 50, 100]) {
                hull() {
                    translate([x-12, DEPTH/2, -1])
                    cylinder(d=5, h=WALL_THICKNESS + 2);
                    translate([x+12, DEPTH/2, -1])
                    cylinder(d=5, h=WALL_THICKNESS + 2);
                }
            }

            // Top exhaust (organic shape around mic)
            translate([0, DEPTH/2, HEIGHT - WALL_THICKNESS/2])
            cylinder(d=MIC_MESH_DIA + 15, h=WALL_THICKNESS + 1, center=true);

            // Back connectors (recessed)
            translate([0, DEPTH - WALL_THICKNESS/2, HEIGHT * 0.45]) {
                // DC jack
                translate([-35, 0, 0])
                rotate([90, 0, 0])
                cylinder(d=11, h=WALL_THICKNESS + 2, center=true);

                // USB-C
                hull() {
                    for (x = [-3.5, 3.5]) {
                        translate([x, 0, 0])
                        rotate([90, 0, 0])
                        cylinder(d=4, h=WALL_THICKNESS + 2, center=true);
                    }
                }

                // SMA antenna
                translate([35, 0, 0])
                rotate([90, 0, 0])
                cylinder(d=7, h=WALL_THICKNESS + 2, center=true);
            }

            // Cutaway
            if (show_cutaway) {
                translate([0, -DEPTH, 0])
                cube([FRONT_WIDTH, DEPTH, HEIGHT * 2], center=true);
            }
        }
    }
}

// ============================================================================
// MODULE: Compact Standoff
// ============================================================================
module standoff(height) {
    color(C_ALUMINUM)
    difference() {
        cylinder(d=STANDOFF_OD, h=height, $fn=6);
        translate([0, 0, -0.1])
        cylinder(d=STANDOFF_ID, h=height + 0.2, $fn=32);
    }
}

// ============================================================================
// MODULE: Compact Shelf Plate
// ============================================================================
module shelf_plate(size, with_vent=true) {
    w = size[0];
    d = size[1];

    color(C_ALUMINUM)
    difference() {
        // Rounded rectangular plate
        hull() {
            for (x = [-1, 1]) {
                for (y = [-1, 1]) {
                    translate([x * (w/2 - 3), y * (d/2 - 3), 0])
                    cylinder(r=3, h=PLATE_THICKNESS);
                }
            }
        }

        // Standoff holes
        for (pos = standoff_positions()) {
            translate([pos[0], pos[1], -0.1])
            cylinder(d=3.2, h=PLATE_THICKNESS + 0.2, $fn=32);
        }

        // Cable ducts (corners, smaller)
        for (x = [-1, 1]) {
            for (y = [-1, 1]) {
                translate([
                    x * (w/2 - CABLE_DUCT_SIZE[0]/2 - 2),
                    y * (d/2 - CABLE_DUCT_SIZE[1]/2 - 2),
                    -0.1
                ])
                cube([CABLE_DUCT_SIZE[0], CABLE_DUCT_SIZE[1], PLATE_THICKNESS + 0.2], center=true);
            }
        }

        // Ventilation (center slot pattern)
        if (with_vent) {
            for (i = [-1, 0, 1]) {
                hull() {
                    translate([i * w/4 - 4, 0, -0.1])
                    cylinder(d=4, h=PLATE_THICKNESS + 0.2);
                    translate([i * w/4 + 4, 0, -0.1])
                    cylinder(d=4, h=PLATE_THICKNESS + 0.2);
                }
            }
        }
    }
}

// ============================================================================
// MODULE: Compact Center Tower (ESP32-P4 Under Level 3)
// ============================================================================
module center_tower() {
    translate([0, DEPTH/2, 0]) {

        // === LEVEL 1: Power + Sensors ===
        translate([0, 0, LEVEL_1_Z]) {
            shelf_plate(PLATE_L1);

            for (pos = standoff_positions()) {
                translate([pos[0], pos[1], PLATE_THICKNESS])
                standoff(STANDOFF_L1_L2);
            }

            // Sensors (compact arrangement)
            color(C_PCB, 0.7)
            translate([0, 0, 6])
            cube([45, 35, 2], center=true);
        }

        // === LEVEL 2: DAC + Amp ===
        translate([0, 0, LEVEL_2_Z]) {
            shelf_plate(PLATE_L2);

            for (pos = standoff_positions()) {
                translate([pos[0], pos[1], PLATE_THICKNESS])
                standoff(STANDOFF_L2_L3);
            }

            // DAC + Amp stacked
            color(C_PCB, 0.7)
            translate([0, 0, 8])
            cube([45, 38, 12], center=true);
        }

        // === LEVEL 3: LCD Mount (ESP32-P4 UNDERNEATH) ===
        translate([0, 0, LEVEL_3_Z]) {
            shelf_plate(PLATE_L3, with_vent=false);

            // Short standoffs to LCD frame
            for (pos = standoff_positions()) {
                translate([pos[0], pos[1], PLATE_THICKNESS])
                standoff(STANDOFF_L3_TOP);
            }

            // ESP32-P4 mounted UNDER Level 3 plate
            color(C_PCB, 0.8)
            translate([0, 0, -8])
            cube([50, 42, 3], center=true);

            // Label
            color([1, 1, 1])
            translate([0, 0, -9.5])
            linear_extrude(0.5)
            text("P4", size=8, halign="center", valign="center");
        }
    }
}

// ============================================================================
// MODULE: Trapezoidal Speaker Box (Following Wedge Angle)
// ============================================================================
module trapezoidal_speaker_box() {
    // Create trapezoidal cross-section
    module trapezoid_2d() {
        polygon([
            [-SPK_FRONT_W/2, 0],
            [SPK_FRONT_W/2, 0],
            [SPK_BACK_W/2, SPK_DEPTH],
            [-SPK_BACK_W/2, SPK_DEPTH]
        ]);
    }

    module inner_trapezoid_2d() {
        t = MDF_THICKNESS;
        front_inner = SPK_FRONT_W - t*2;
        back_inner = SPK_BACK_W - t*2;
        polygon([
            [-front_inner/2, t],
            [front_inner/2, t],
            [back_inner/2, SPK_DEPTH - t],
            [-back_inner/2, SPK_DEPTH - t]
        ]);
    }

    color(C_MDF) {
        difference() {
            // Outer trapezoidal prism
            linear_extrude(height=SPK_HEIGHT)
            trapezoid_2d();

            // Inner cavity
            translate([0, 0, MDF_THICKNESS])
            linear_extrude(height=SPK_HEIGHT - MDF_THICKNESS*2)
            inner_trapezoid_2d();

            // Main driver cutout (front)
            translate([0, -1, SPK_HEIGHT * 0.62])
            rotate([-90, 0, 0])
            cylinder(d=DRIVER_DIA, h=MDF_THICKNESS + 2);

            // Passive radiator (front, below driver)
            translate([0, -1, SPK_HEIGHT * 0.28])
            rotate([-90, 0, 0])
            cylinder(d=PASSIVE_RAD_DIA, h=MDF_THICKNESS + 2);

            // Terminal cup (back)
            translate([0, SPK_DEPTH - MDF_THICKNESS - 1, SPK_HEIGHT * 0.5])
            rotate([-90, 0, 0])
            cylinder(d=35, h=MDF_THICKNESS + 2);
        }
    }

    // Hanenite pad
    color(C_RUBBER)
    translate([0, 0, -HANENITE_THICKNESS])
    linear_extrude(height=HANENITE_THICKNESS)
    trapezoid_2d();
}

// ============================================================================
// MODULE: Speaker Box Pair (Trapezoidal, Toe-in)
// ============================================================================
module speaker_boxes() {
    // Left speaker
    translate([-SPK_X_OFFSET, SPK_Y_OFFSET, SPK_Z_OFFSET])
    rotate([0, 0, 8])  // Slight toe-in
    trapezoidal_speaker_box();

    // Right speaker (mirrored)
    translate([SPK_X_OFFSET, SPK_Y_OFFSET, SPK_Z_OFFSET])
    rotate([0, 0, -8])
    mirror([1, 0, 0])
    trapezoidal_speaker_box();
}

// ============================================================================
// MODULE: LCD Assembly (Inset 10mm)
// ============================================================================
module lcd_assembly() {
    // Inset position (10mm from front panel)
    translate([0, WALL_THICKNESS + LCD_Y_INSET, LCD_Z_CENTER]) {
        // LCD module
        color(C_LCD)
        cube([LCD_MODULE[0], LCD_MODULE[2], LCD_MODULE[1]], center=true);

        // Active display
        color([0.12, 0.15, 0.20])
        translate([0, -2, 0])
        cube([LCD_ACTIVE[0], 1, LCD_ACTIVE[1]], center=true);

        // Curved bezel frame (follows front panel curve)
        color(C_FRAME) {
            difference() {
                // Bezel body
                translate([0, -LCD_Y_INSET/2, 0])
                hull() {
                    // Front edge (curved to match shell)
                    for (x = [-1, 1]) {
                        for (z = [-1, 1]) {
                            translate([x * (LCD_BEZEL[0]/2 - 6), -LCD_Y_INSET/2, z * (LCD_BEZEL[1]/2 - 6)])
                            rotate([90, 0, 0])
                            cylinder(r=6, h=LCD_BEZEL[2]);
                        }
                    }
                    // Back edge (straight, against LCD)
                    translate([0, LCD_Y_INSET/2 - 2, 0])
                    cube([LCD_MODULE[0] + 6, 2, LCD_MODULE[1] + 6], center=true);
                }

                // LCD cutout
                cube([LCD_MODULE[0] + 2, LCD_Y_INSET + 10, LCD_MODULE[1] + 2], center=true);
            }
        }
    }
}

// ============================================================================
// MODULE: XVF3800 (Top Mount, Streamlined)
// ============================================================================
module xvf3800() {
    translate([0, DEPTH/2, MIC_Z]) {
        // PCB
        color(C_PCB)
        hull() {
            for (x = [-1, 1]) {
                for (y = [-1, 1]) {
                    translate([x * (MIC_PCB[0]/2 - 4), y * (MIC_PCB[1]/2 - 4), 0])
                    cylinder(r=4, h=MIC_PCB[2], center=true);
                }
            }
        }

        // MEMS mics
        color([0.35, 0.35, 0.40])
        for (x = [-10, 10]) {
            for (y = [-10, 10]) {
                translate([x, y, MIC_PCB[2]/2 + 0.8])
                cylinder(d=4, h=1.5);
            }
        }

        // Acoustic mesh (flush with top)
        color(C_MESH)
        translate([0, 0, MIC_PCB[2]/2 + 3])
        cylinder(d=MIC_MESH_DIA, h=0.8);
    }
}

// ============================================================================
// MODULE: Fabric Grille (Formation Wedge Style)
// ============================================================================
module fabric_grille() {
    color(C_FABRIC, 0.85) {
        difference() {
            // Full front face coverage
            translate([0, 1, HEIGHT/2])
            rotate([90, 0, 0])
            linear_extrude(height=2)
            hull() {
                for (x = [-1, 1]) {
                    translate([x * (FRONT_WIDTH/2 - CORNER_RADIUS - 5), 0])
                    circle(r=5);
                    translate([x * (FRONT_WIDTH/2 - CORNER_RADIUS - 5), HEIGHT - CORNER_RADIUS*2])
                    circle(r=5);
                }
            }

            // LCD opening
            translate([0, 2, LCD_Z_CENTER])
            rotate([90, 0, 0])
            hull() {
                for (x = [-1, 1]) {
                    for (z = [-1, 1]) {
                        translate([x * (LCD_BEZEL[0]/2 - 10), z * (LCD_BEZEL[1]/2 - 10)])
                        cylinder(r=10, h=4);
                    }
                }
            }
        }
    }
}

// ============================================================================
// MODULE: Inner Frame (Streamlined)
// ============================================================================
module inner_frame() {
    color(C_FRAME, 0.6) {
        // Tower base (compact)
        translate([0, DEPTH/2, WALL_THICKNESS]) {
            difference() {
                cylinder(d=PLATE_L1[0] + 15, h=8);
                cylinder(d=PLATE_L1[0] - 8, h=9);
            }
        }

        // Speaker cradles (follow trapezoid)
        for (side = [-1, 1]) {
            translate([side * SPK_X_OFFSET, SPK_Y_OFFSET + SPK_DEPTH/2, SPK_Z_OFFSET - 3])
            rotate([0, 0, side * 8])
            difference() {
                cube([12, SPK_DEPTH - 30, 8], center=true);
                cube([8, SPK_DEPTH - 20, 10], center=true);
            }
        }

        // LCD frame rails
        translate([0, WALL_THICKNESS + LCD_Y_INSET, LCD_Z_CENTER])
        rotate([90, 0, 0])
        difference() {
            hull() {
                for (x = [-1, 1]) {
                    translate([x * (LCD_BEZEL[0]/2 + 3), 0, 0])
                    cube([6, LCD_BEZEL[1] + 6, 6], center=true);
                }
            }
            cube([LCD_BEZEL[0], LCD_BEZEL[1], 10], center=true);
        }

        // Mic mount (flush ring)
        translate([0, DEPTH/2, HEIGHT - 12])
        difference() {
            cylinder(d=MIC_MESH_DIA + 10, h=6);
            cylinder(d=MIC_PCB[0] + 1, h=8);
            // Ventilation slots
            for (a = [0, 90, 180, 270]) {
                rotate([0, 0, a + 45])
                translate([MIC_MESH_DIA/2 + 2, 0, 0])
                cylinder(d=4, h=8);
            }
        }
    }
}

// ============================================================================
// ASSEMBLY MODES
// ============================================================================

module full_assembly() {
    outer_shell_rounded(show_cutaway=false);
    center_tower();
    speaker_boxes();
    lcd_assembly();
    xvf3800();
    inner_frame();
    fabric_grille();
}

module exploded_view() {
    translate([0, 0, 0]) outer_shell_rounded(show_cutaway=true);
    translate([0, 0, 60]) center_tower();
    translate([0, 0, 40]) speaker_boxes();
    translate([0, 0, 90]) lcd_assembly();
    translate([0, 0, 110]) xvf3800();
}

module section_view() {
    difference() {
        full_assembly();
        translate([0, -200, -10])
        cube([400, 200, 300]);
    }
}

module internal_only() {
    center_tower();
    speaker_boxes();
    lcd_assembly();
    xvf3800();
    inner_frame();
}

module top_view() {
    projection(cut=true)
    translate([0, 0, -HEIGHT/2])
    full_assembly();
}

// ============================================================================
// RENDER SELECTION
// ============================================================================

// Uncomment one to render:

full_assembly();              // Complete Formation Wedge style
// exploded_view();           // Exploded view
// section_view();            // Cross-section
// internal_only();           // Internal components only
// top_view();                // 2D top view

// ============================================================================
// EXPORT NOTES
// ============================================================================
/*
v3.0 Formation Wedge Slim Design:

CHANGES FROM v2.0:
1. Speaker boxes: Trapezoidal (follows 120° wedge)
   - Front width: 95mm, Back width: 70mm
   - Better volume utilization, slimmer profile

2. Tower: Compact 55mm pitch
   - ESP32-P4 mounted UNDER Level 3 plate
   - Smaller footprint, more speaker room

3. LCD: 10mm inset
   - Curved bezel blends with front panel
   - Better visual integration

4. Corners: Rounded with offset(r=10)
   - Formation Wedge aesthetic
   - $fn=128 for smooth curves

SPEAKER BOX VOLUME:
  Approximate: (95+70)/2 × 170 × 130 × 0.5 = ~1.8L per side
  (Still adequate for Peerless + passive radiator)

TOWER CLEARANCE:
  With 55mm pitch, tower envelope: ~80mm diameter
  Speaker boxes start at X=±115mm
  Clearance: 115 - 40 - 47.5 = 27.5mm per side (OK)
*/
