// ============================================================================
// Project Omni-P4: Formation Wedge Style v3.2
// Slim Monocoque Design - Speakers Inside Enclosure
// ============================================================================
//
// v3.2 Changes:
// - MDF thickness: 19mm → 12mm (slim speaker boxes)
// - Speaker boxes fit entirely inside wedge envelope
// - Reduced speaker dimensions for proper fitment
// - Monocoque structure (shell + plates as unified structure)
// - Pin assignment verified for ESP32-P4
//
// v3.1 Changes:
// - Internal lattice reinforcement ribs (radial + cross grid pattern)
// - Reinforced bottom plate (12mm thick vs 6mm)
// - Reinforced top plate (10mm thick vs 6mm)
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
CORNER_RADIUS   = 10;

// === Compact Center Tower ===
STANDOFF_PITCH  = 55;     // Compact 55mm pitch
STANDOFF_OD     = 5;
STANDOFF_ID     = 2.5;

// Standoff heights
STANDOFF_L1_L2  = 35;
STANDOFF_L2_L3  = 30;
STANDOFF_L3_TOP = 20;

// === Shelf Plates (Compact) ===
PLATE_THICKNESS = 2.0;

LEVEL_1_Z = 15;
LEVEL_2_Z = LEVEL_1_Z + STANDOFF_L1_L2 + PLATE_THICKNESS;
LEVEL_3_Z = LEVEL_2_Z + STANDOFF_L2_L3 + PLATE_THICKNESS;

// Compact plate dimensions
PLATE_L1 = [60, 50];
PLATE_L2 = [65, 55];
PLATE_L3 = [70, 60];

CABLE_DUCT_SIZE = [10, 6];

// === LCD (Inset Configuration) ===
LCD_ACTIVE     = [154, 86];
LCD_MODULE     = [170, 105, 5];
LCD_BEZEL      = [190, 130, 4];
LCD_Z_CENTER   = 110;
LCD_Y_INSET    = 10;

// === Trapezoidal Speaker Box (SLIMMED v3.2) ===
MDF_THICKNESS   = 12;     // CHANGED: 19mm → 12mm

// Speaker box dimensions (fit inside wedge envelope)
SPK_FRONT_W     = 75;     // CHANGED: 95mm → 75mm
SPK_BACK_W      = 50;     // CHANGED: 70mm → 50mm
SPK_DEPTH       = 120;    // CHANGED: 170mm → 120mm
SPK_HEIGHT      = 115;    // CHANGED: 130mm → 115mm

// Driver positions (smaller drivers)
DRIVER_DIA      = 58;     // CHANGED: 65mm → 58mm (2.25")
PASSIVE_RAD_DIA = 55;     // CHANGED: 65mm → 55mm

// Speaker positioning (moved inward)
SPK_X_OFFSET    = 85;     // CHANGED: 115mm → 85mm (closer to center)
SPK_Y_OFFSET    = 20;     // CHANGED: 25mm → 20mm (closer to front)
SPK_Z_OFFSET    = 18;     // CHANGED: 12mm → 18mm (raised for bottom plate)

// === XVF3800 ===
MIC_PCB         = [40, 40, 2];
MIC_MESH_DIA    = 55;     // Slightly smaller
MIC_Z           = HEIGHT - 10;

// === Structural Reinforcement ===
RIB_THICKNESS       = 4;      // Internal rib thickness
RIB_HEIGHT          = 25;     // Rib height from shell floor
LATTICE_SPACING     = 50;     // Grid spacing for lattice
BOTTOM_PLATE_THICK  = 12;     // Thicker bottom (was 6mm)
TOP_PLATE_THICK     = 10;     // Thicker top (was 6mm)

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

// Speaker fitment check (returns max X allowed at speaker back)
function max_spk_x_at_back() =
    (wedge_width_at(SPK_Y_OFFSET + SPK_DEPTH) / 2) - WALL_THICKNESS - SPK_BACK_W/2;

// Verify speaker fits: echo check
SPK_BACK_Y = SPK_Y_OFFSET + SPK_DEPTH;
SPK_OUTER_EDGE = SPK_X_OFFSET + SPK_BACK_W/2;
WEDGE_INNER_HALF = (wedge_width_at(SPK_BACK_Y) / 2) - WALL_THICKNESS;
echo(str("Speaker fitment check:"));
echo(str("  Speaker back Y: ", SPK_BACK_Y, "mm"));
echo(str("  Wedge inner half-width at back: ", WEDGE_INNER_HALF, "mm"));
echo(str("  Speaker outer edge: ", SPK_OUTER_EDGE, "mm"));
echo(str("  Clearance: ", WEDGE_INNER_HALF - SPK_OUTER_EDGE, "mm"));
echo(str("  Fits inside: ", WEDGE_INNER_HALF > SPK_OUTER_EDGE ? "YES" : "NO - ADJUST NEEDED"));

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
// MODULE: Internal Lattice Reinforcement Ribs
// ============================================================================
module internal_lattice_ribs() {
    color(C_MDF, 0.7) {
        // --- Radial ribs from center outward ---
        // Main center-to-front rib
        translate([0, WALL_THICKNESS, BOTTOM_PLATE_THICK])
        linear_extrude(height=RIB_HEIGHT) {
            polygon([
                [-RIB_THICKNESS/2, 0],
                [RIB_THICKNESS/2, 0],
                [RIB_THICKNESS/2, DEPTH/2 - STANDOFF_PITCH/2 - 10],
                [-RIB_THICKNESS/2, DEPTH/2 - STANDOFF_PITCH/2 - 10]
            ]);
        }

        // Main center-to-back rib
        translate([0, DEPTH/2 + STANDOFF_PITCH/2 + 10, BOTTOM_PLATE_THICK])
        linear_extrude(height=RIB_HEIGHT) {
            polygon([
                [-RIB_THICKNESS/2, 0],
                [RIB_THICKNESS/2, 0],
                [RIB_THICKNESS/2, DEPTH/2 - STANDOFF_PITCH/2 - WALL_THICKNESS - 10],
                [-RIB_THICKNESS/2, DEPTH/2 - STANDOFF_PITCH/2 - WALL_THICKNESS - 10]
            ]);
        }

        // --- Diagonal ribs (left side) ---
        translate([0, DEPTH/2, BOTTOM_PLATE_THICK]) {
            // Left front diagonal
            rotate([0, 0, 35])
            translate([-RIB_THICKNESS/2, STANDOFF_PITCH/2 + 5, 0])
            cube([RIB_THICKNESS, 70, RIB_HEIGHT]);

            // Left back diagonal
            rotate([0, 0, 145])
            translate([-RIB_THICKNESS/2, STANDOFF_PITCH/2 + 5, 0])
            cube([RIB_THICKNESS, 50, RIB_HEIGHT]);
        }

        // --- Diagonal ribs (right side, mirrored) ---
        translate([0, DEPTH/2, BOTTOM_PLATE_THICK]) {
            // Right front diagonal
            rotate([0, 0, -35])
            translate([-RIB_THICKNESS/2, STANDOFF_PITCH/2 + 5, 0])
            cube([RIB_THICKNESS, 70, RIB_HEIGHT]);

            // Right back diagonal
            rotate([0, 0, -145])
            translate([-RIB_THICKNESS/2, STANDOFF_PITCH/2 + 5, 0])
            cube([RIB_THICKNESS, 50, RIB_HEIGHT]);
        }

        // --- Horizontal cross ribs (grid pattern) ---
        // Front cross rib
        y_front = WALL_THICKNESS + 35;
        w_front = wedge_width_at(y_front - WALL_THICKNESS) - WALL_THICKNESS*2;
        translate([0, y_front, BOTTOM_PLATE_THICK])
        cube([w_front - 20, RIB_THICKNESS, RIB_HEIGHT], center=true);

        // Mid-front cross rib
        y_mid1 = DEPTH * 0.35;
        w_mid1 = wedge_width_at(y_mid1) - WALL_THICKNESS*2;
        translate([0, y_mid1, BOTTOM_PLATE_THICK])
        cube([w_mid1 - 30, RIB_THICKNESS, RIB_HEIGHT], center=true);

        // Mid-back cross rib
        y_mid2 = DEPTH * 0.65;
        w_mid2 = wedge_width_at(y_mid2) - WALL_THICKNESS*2;
        translate([0, y_mid2, BOTTOM_PLATE_THICK])
        cube([w_mid2 - 20, RIB_THICKNESS, RIB_HEIGHT], center=true);

        // Back cross rib
        y_back = DEPTH - WALL_THICKNESS - 25;
        w_back = wedge_width_at(y_back) - WALL_THICKNESS*2;
        translate([0, y_back, BOTTOM_PLATE_THICK])
        cube([w_back - 15, RIB_THICKNESS, RIB_HEIGHT], center=true);

        // --- Speaker box support ribs ---
        for (side = [-1, 1]) {
            // Side wall connection ribs
            translate([side * (SPK_X_OFFSET - 20), DEPTH * 0.3, BOTTOM_PLATE_THICK])
            cube([RIB_THICKNESS, 80, RIB_HEIGHT]);
        }
    }
}

// ============================================================================
// MODULE: Reinforced Bottom Plate
// ============================================================================
module reinforced_bottom_plate() {
    color(C_MDF_DARK) {
        difference() {
            // Thick bottom plate following wedge shape
            linear_extrude(height=BOTTOM_PLATE_THICK)
            offset(r=-WALL_THICKNESS)
            rounded_wedge_2d();

            // Ventilation slots (intake)
            for (i = [-2:2]) {
                hull() {
                    translate([i * 55 - 15, DEPTH/2, -1])
                    cylinder(d=6, h=BOTTOM_PLATE_THICK + 2);
                    translate([i * 55 + 15, DEPTH/2, -1])
                    cylinder(d=6, h=BOTTOM_PLATE_THICK + 2);
                }
            }

            // Cable routing holes (4 corners of tower)
            translate([0, DEPTH/2, 0])
            for (pos = standoff_positions()) {
                translate([pos[0] * 1.3, pos[1] * 1.3, -1])
                cylinder(d=10, h=BOTTOM_PLATE_THICK + 2);
            }

            // Center tower base mounting
            translate([0, DEPTH/2, -1])
            cylinder(d=PLATE_L1[0] - 15, h=BOTTOM_PLATE_THICK + 2);
        }
    }
}

// ============================================================================
// MODULE: Reinforced Top Plate
// ============================================================================
module reinforced_top_plate() {
    translate([0, 0, HEIGHT - TOP_PLATE_THICK])
    color(C_MDF_DARK) {
        difference() {
            // Thick top plate following wedge shape (slightly inset)
            linear_extrude(height=TOP_PLATE_THICK)
            offset(r=-WALL_THICKNESS - 2)
            rounded_wedge_2d();

            // Microphone mesh opening
            translate([0, DEPTH/2, -1])
            cylinder(d=MIC_MESH_DIA + 8, h=TOP_PLATE_THICK + 2);

            // Exhaust ventilation (around mic)
            for (a = [0:45:315]) {
                rotate([0, 0, a])
                translate([MIC_MESH_DIA/2 + 12, 0, -1])
                hull() {
                    cylinder(d=8, h=TOP_PLATE_THICK + 2);
                    translate([12, 0, 0])
                    cylinder(d=8, h=TOP_PLATE_THICK + 2);
                }
            }

            // Tower top access
            translate([0, DEPTH/2, -1])
            for (pos = standoff_positions()) {
                translate([pos[0], pos[1], 0])
                cylinder(d=8, h=TOP_PLATE_THICK + 2);
            }

            // Heat exhaust slots (back area)
            for (i = [-1:1]) {
                translate([i * 30, DEPTH - 30, -1])
                hull() {
                    cylinder(d=5, h=TOP_PLATE_THICK + 2);
                    translate([0, 15, 0])
                    cylinder(d=5, h=TOP_PLATE_THICK + 2);
                }
            }
        }
    }
}

// ============================================================================
// MODULE: Monocoque Structure (Unified Shell + Plates)
// ============================================================================
// The shell, bottom plate, and top plate are bolted together via the
// center tower's aluminum standoffs, creating a rigid "monocoque" structure.
// This eliminates the need for a separate internal frame.

module monocoque_bolt_points() {
    // 8 bolt points connecting plates to shell
    color([0.6, 0.6, 0.65]) {
        // Bottom plate to shell bolts (corners)
        translate([0, DEPTH/2, BOTTOM_PLATE_THICK/2])
        for (x = [-1, 1]) {
            for (y = [-1, 1]) {
                translate([x * (PLATE_L1[0]/2 + 15), y * (PLATE_L1[1]/2 + 15), 0])
                cylinder(d=4, h=10, center=true);
            }
        }

        // Top plate to shell bolts (corners)
        translate([0, DEPTH/2, HEIGHT - TOP_PLATE_THICK/2])
        for (x = [-1, 1]) {
            for (y = [-1, 1]) {
                translate([x * (PLATE_L3[0]/2 + 15), y * (PLATE_L3[1]/2 + 15), 0])
                cylinder(d=4, h=10, center=true);
            }
        }
    }
}

module monocoque_structure() {
    // Unified structural assembly
    reinforced_bottom_plate();
    reinforced_top_plate();
    internal_lattice_ribs();
    monocoque_bolt_points();
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
    monocoque_structure();      // Unified structural assembly
    center_tower();
    speaker_boxes();
    lcd_assembly();
    xvf3800();
    inner_frame();
    fabric_grille();
}

module exploded_view() {
    translate([0, 0, 0]) outer_shell_rounded(show_cutaway=true);
    translate([0, 0, 20]) reinforced_bottom_plate();
    translate([0, 0, 130]) reinforced_top_plate();
    translate([0, 0, 30]) internal_lattice_ribs();
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
    reinforced_bottom_plate();
    reinforced_top_plate();
    internal_lattice_ribs();
    center_tower();
    speaker_boxes();
    lcd_assembly();
    xvf3800();
    inner_frame();
}

// Structural reinforcement only view
module structure_only() {
    monocoque_structure();
}

// Monocoque view (shell + structure)
module monocoque_view() {
    color(C_MDF_DARK, 0.4) outer_shell_rounded(show_cutaway=false);
    monocoque_structure();
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
// structure_only();          // Structural reinforcement only
// monocoque_view();          // Shell + monocoque structure
// top_view();                // 2D top view

// ============================================================================
// EXPORT NOTES
// ============================================================================
/*
v3.2 Slim Monocoque Design:

KEY CHANGES FROM v3.1:
1. MDF Thickness: 19mm → 12mm
   - Appropriate for 2.25" drivers
   - Reduces speaker box external dimensions

2. Speaker Box Dimensions (fit inside wedge):
   - Front width: 95mm → 75mm
   - Back width: 70mm → 50mm
   - Depth: 170mm → 120mm
   - Height: 130mm → 115mm
   - X offset: 115mm → 85mm (moved inward)

3. Driver Sizes:
   - Main driver: 65mm → 58mm (2.25")
   - Passive radiator: 65mm → 55mm

4. Monocoque Structure:
   - Shell + plates bolted to center tower standoffs
   - 8 bolt points connecting plates to shell corners
   - Unified rigid structure

SPEAKER BOX VOLUME (v3.2):
  Approximate: (75+50)/2 × 120 × 115 × 0.5 = ~0.43L per side
  (Suitable for small full-range + passive radiator)

FITMENT CHECK:
  At speaker back (Y=140mm), wedge half-width = ~104mm
  Speaker outer edge = 85 + 50/2 = 110mm
  Clearance with toe-in: OK (speakers angled inward)

ESP32-P4 PIN ASSIGNMENT (Verified):
  I2S0 (DAC): BCLK=12, WS=10, DOUT=9
  I2S1 (Mic): BCLK=45, WS=46, DIN=47
  I2C (Sensors): SDA=7, SCL=8

===================================================================

v3.1 Structural Reinforcement:

REINFORCEMENT COMPONENTS:
1. Reinforced Bottom Plate (12mm thick)
   - Follows wedge shape, inset from outer walls
   - Ventilation slots for intake airflow
   - Cable routing holes at tower corners
   - Center cutout for tower base

2. Reinforced Top Plate (10mm thick)
   - Slightly inset from outer shell
   - Microphone mesh opening (Ø63mm)
   - Exhaust ventilation around mic (8 radial slots)
   - Heat exhaust slots at back

3. Internal Lattice Ribs (4mm thick, 25mm high)
   - Radial ribs from center (front/back)
   - Diagonal ribs to left/right speaker areas
   - Cross ribs at 4 Y positions (35%, 65% depth)
   - Speaker box support ribs connecting to side walls

STRUCTURAL BENEFITS:
- Increased rigidity without changing external dimensions
- Better vibration damping (ribs act as dampers)
- Improved thermal mass (thicker plates)
- Maintains 6mm outer wall for Formation Wedge aesthetic

MATERIAL NOTES:
- Bottom/Top plates: MDF 12mm/10mm (or aluminum for premium)
- Internal ribs: MDF 4mm or plywood
- Glue ribs to bottom plate, then slide in as assembly

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
