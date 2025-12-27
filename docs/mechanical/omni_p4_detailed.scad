// ============================================================================
// Project Omni-P4: Formation Wedge Style v3.5
// Elliptical Front + 120° Wedge Top View
// ============================================================================
//
// v3.5 Changes:
// - Front view: Elliptical profile (smooth oval)
// - Top view: 120-degree wedge/fan shape maintained
// - Lofted shell using hull() of elliptical cross-sections
// - Internal structure preserved unchanged
//
// v3.4 Changes:
// - BACK_WIDTH: 150mm → 30mm (true wedge/triangular shape)
// - Back corner radius: 30mm (smooth continuous curve)
// - Center tower moved forward to avoid back protrusion
//
// ============================================================================

$fn = 128;  // High resolution for smooth curves

// ============================================================================
// MASTER PARAMETERS
// ============================================================================

// === Outer Shell (Elliptical Wedge) ===
FRONT_WIDTH       = 380;      // Major axis of front ellipse
BACK_WIDTH        = 30;       // Back tip width (nearly pointed)
DEPTH             = 200;      // Front to back
HEIGHT            = 180;      // Vertical (minor axis of front ellipse)
WALL_THICKNESS    = 8;
WEDGE_ANGLE       = 120;      // Internal angle at back vertex
CORNER_RADIUS     = 15;       // Front corner radius (legacy, for rounded_wedge_2d)
BACK_CORNER_RADIUS = 30;      // Back tip radius

// Elliptical wedge parameters
ELLIPSE_STEPS     = 8;        // Number of hull() slices for smooth loft
BACK_ELLIPSE_SCALE = 0.15;    // How much smaller the back ellipse is (height)

// === Center Tower (Moved Forward) ===
STANDOFF_PITCH    = 55;
STANDOFF_OD       = 5;
STANDOFF_ID       = 2.5;
TOWER_Y_OFFSET    = -25;      // NEW: Move tower 25mm forward from center

// Standoff heights
STANDOFF_L1_L2    = 35;
STANDOFF_L2_L3    = 30;
STANDOFF_L3_TOP   = 20;

// === Shelf Plates (Compact) ===
PLATE_THICKNESS = 2.0;

LEVEL_1_Z = 15;
LEVEL_2_Z = LEVEL_1_Z + STANDOFF_L1_L2 + PLATE_THICKNESS;
LEVEL_3_Z = LEVEL_2_Z + STANDOFF_L2_L3 + PLATE_THICKNESS;

// Compact plate dimensions
PLATE_L1 = [60, 50];
PLATE_L2 = [65, 55];
PLATE_L3 = [70, 60];

// Cable duct with I2S channel separation
CABLE_DUCT_SIZE = [12, 8];    // Enlarged for shielded cables
I2S_CHANNEL_W   = 5;          // Separate I2S DAC/MIC channels

// === LCD (B&O Inset Configuration with Shadow Gap) ===
LCD_ACTIVE      = [154, 86];
LCD_MODULE      = [170, 105, 5];
LCD_BEZEL       = [195, 135, 6];  // Larger bezel for taper
LCD_Z_CENTER    = 110;
LCD_Y_INSET     = 12;             // CHANGED: 10mm → 12mm (deeper inset)
LCD_SHADOW_GAP  = 1.0;            // NEW: B&O floating effect
LCD_TAPER_ANGLE = 15;             // NEW: Bezel taper angle (degrees)

// === Trapezoidal Speaker Box (True Wedge v3.4) ===
MDF_THICKNESS   = 12;

// Speaker box dimensions (adjusted for narrow wedge)
// With BACK_WIDTH=30mm, speakers must be more forward
SPK_FRONT_W     = 100;    // Slightly narrower
SPK_BACK_W      = 55;     // Narrower to follow wedge taper
SPK_DEPTH       = 120;    // CHANGED: 150mm → 120mm (shorter to fit narrow back)
SPK_HEIGHT      = 145;

// Driver positions
DRIVER_DIA      = 65;
PASSIVE_RAD_DIA = 60;

// Speaker positioning (adjusted for true wedge)
SPK_X_OFFSET    = 75;     // CHANGED: 68mm → 75mm (more outward)
SPK_Y_OFFSET    = 12;     // CHANGED: 15mm → 12mm (more forward)
SPK_Z_OFFSET    = 18;
SPK_TOE_IN      = 15;     // NEW: 8° → 15° (follows wedge angle)

// === XVF3800 ===
MIC_PCB         = [40, 40, 2];
MIC_MESH_DIA    = 55;
MIC_Z           = HEIGHT - 12;

// === Structural Reinforcement (Enhanced) ===
RIB_THICKNESS       = 5;      // CHANGED: 4mm → 5mm (stronger ribs)
RIB_HEIGHT          = 30;     // CHANGED: 25mm → 30mm
LATTICE_SPACING     = 45;     // CHANGED: 50mm → 45mm (denser grid)
BOTTOM_PLATE_THICK  = 12;
TOP_PLATE_THICK     = 10;

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

// Calculate ellipse height at given Y position (tapers toward back)
function ellipse_height_at(y) =
    HEIGHT * (1 - (y / DEPTH) * BACK_ELLIPSE_SCALE);

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

// Speaker volume calculation (internal, accounting for MDF walls)
SPK_INTERNAL_FRONT = SPK_FRONT_W - MDF_THICKNESS * 2;
SPK_INTERNAL_BACK = SPK_BACK_W - MDF_THICKNESS * 2;
SPK_INTERNAL_DEPTH = SPK_DEPTH - MDF_THICKNESS * 2;
SPK_INTERNAL_HEIGHT = SPK_HEIGHT - MDF_THICKNESS * 2;
SPK_VOLUME_ML = (SPK_INTERNAL_FRONT + SPK_INTERNAL_BACK) / 2 * SPK_INTERNAL_DEPTH * SPK_INTERNAL_HEIGHT / 1000;
SPK_VOLUME_L = SPK_VOLUME_ML / 1000;

echo(str("=== v3.5 Elliptical Wedge Design Verification ==="));
echo(str("Elliptical wedge geometry:"));
echo(str("  Front width (ellipse major axis): ", FRONT_WIDTH, "mm"));
echo(str("  Back width: ", BACK_WIDTH, "mm (nearly pointed)"));
echo(str("  Height (ellipse minor axis): ", HEIGHT, "mm"));
echo(str("  Back height: ", ellipse_height_at(DEPTH), "mm"));
echo(str("  Taper ratio: ", (FRONT_WIDTH - BACK_WIDTH) / DEPTH, "mm/mm"));
echo(str("  Wedge angle: ", WEDGE_ANGLE, "° (internal)"));
echo(str("Speaker fitment check:"));
echo(str("  Speaker back Y: ", SPK_BACK_Y, "mm"));
echo(str("  Wedge inner half-width at back: ", WEDGE_INNER_HALF, "mm"));
echo(str("  Speaker outer edge: ", SPK_OUTER_EDGE, "mm"));
echo(str("  Clearance: ", WEDGE_INNER_HALF - SPK_OUTER_EDGE, "mm"));
echo(str("  Fits inside: ", WEDGE_INNER_HALF > SPK_OUTER_EDGE ? "YES" : "NO - ADJUST NEEDED"));
echo(str("Speaker volume:"));
echo(str("  Volume per channel: ", SPK_VOLUME_L, "L"));
echo(str("  Toe-in angle: ", SPK_TOE_IN, "°"));
echo(str("Shell: ", WALL_THICKNESS, "mm wall, ", ELLIPSE_STEPS, " loft steps"));

// ============================================================================
// MODULE: True Wedge Shape with Smooth Back Curve
// ============================================================================

// Create smooth wedge with large back radius
module rounded_wedge_2d() {
    hull() {
        // Front left corner
        translate([-FRONT_WIDTH/2 + CORNER_RADIUS, CORNER_RADIUS])
        circle(r=CORNER_RADIUS);

        // Front right corner
        translate([FRONT_WIDTH/2 - CORNER_RADIUS, CORNER_RADIUS])
        circle(r=CORNER_RADIUS);

        // Back - single large arc for smooth curve
        translate([0, DEPTH - BACK_CORNER_RADIUS])
        circle(r=BACK_CORNER_RADIUS);
    }
}

// Inner wedge (for cavity)
module inner_wedge_2d() {
    hull() {
        // Front left corner (inset)
        translate([-FRONT_WIDTH/2 + CORNER_RADIUS + WALL_THICKNESS, CORNER_RADIUS + WALL_THICKNESS])
        circle(r=CORNER_RADIUS);

        // Front right corner (inset)
        translate([FRONT_WIDTH/2 - CORNER_RADIUS - WALL_THICKNESS, CORNER_RADIUS + WALL_THICKNESS])
        circle(r=CORNER_RADIUS);

        // Back - single arc (inset)
        translate([0, DEPTH - BACK_CORNER_RADIUS - WALL_THICKNESS])
        circle(r=max(BACK_CORNER_RADIUS - WALL_THICKNESS, 5));
    }
}

// ============================================================================
// MODULE: Elliptical Wedge 2D Shape (for plates)
// Creates 2D cross-section of elliptical wedge at XY plane
// ============================================================================
module elliptical_wedge_2d(inset=0) {
    // Create wedge shape by connecting ellipse center points
    // Uses hull() with multiple circles along the wedge taper
    hull() {
        // Front - wide ellipse (represented as circle at front)
        front_w = FRONT_WIDTH - inset * 2;
        translate([-front_w/2 + 15, 15])
        circle(r=15);
        translate([front_w/2 - 15, 15])
        circle(r=15);

        // Mid points along wedge
        for (i = [1:4]) {
            y = DEPTH * i / 5;
            w = wedge_width_at(y) - inset * 2;
            r = max(10 - i * 1.5, 3);
            translate([0, y])
            circle(r=r);
        }

        // Back tip
        translate([0, DEPTH - BACK_CORNER_RADIUS - inset])
        circle(r=max(BACK_CORNER_RADIUS - inset, 5));
    }
}

module outer_shell_rounded(show_cutaway=false) {
    color(C_MDF_DARK, 0.35) {
        difference() {
            // Rounded outer wedge with smooth back
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

            // Inner cavity (follows wedge taper)
            translate([0, 0, WALL_THICKNESS])
            linear_extrude(height=HEIGHT - WALL_THICKNESS*2)
            inner_wedge_2d();

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

            // Bottom intake vents (forward position, avoid narrow back)
            for (x = [-100, -50, 0, 50, 100]) {
                hull() {
                    translate([x-12, DEPTH * 0.35, -1])
                    cylinder(d=5, h=WALL_THICKNESS + 2);
                    translate([x+12, DEPTH * 0.35, -1])
                    cylinder(d=5, h=WALL_THICKNESS + 2);
                }
            }

            // Top exhaust (moved forward due to narrow back)
            translate([0, DEPTH * 0.4, HEIGHT - WALL_THICKNESS/2])
            cylinder(d=MIC_MESH_DIA + 15, h=WALL_THICKNESS + 1, center=true);

            // Side connectors (moved from narrow back to sides)
            // Left side: DC jack + USB-C
            translate([-FRONT_WIDTH/4, 0, HEIGHT * 0.35]) {
                // Connector panel area on angled side
                rotate([0, 0, -30])  // Follow wedge angle
                translate([0, -1, 0]) {
                    // DC jack
                    translate([0, 0, 15])
                    rotate([-90, 0, 0])
                    cylinder(d=11, h=WALL_THICKNESS + 2);

                    // USB-C
                    translate([0, 0, -10])
                    rotate([-90, 0, 0])
                    hull() {
                        for (x = [-3.5, 3.5]) {
                            translate([x, 0, 0])
                            cylinder(d=4, h=WALL_THICKNESS + 2);
                        }
                    }
                }
            }

            // Right side: SMA antenna
            translate([FRONT_WIDTH/4, 0, HEIGHT * 0.35])
            rotate([0, 0, 30])
            translate([0, -1, 0])
            rotate([-90, 0, 0])
            cylinder(d=7, h=WALL_THICKNESS + 2);

            // Cutaway
            if (show_cutaway) {
                translate([0, -DEPTH, 0])
                cube([FRONT_WIDTH, DEPTH, HEIGHT * 2], center=true);
            }
        }
    }
}

// ============================================================================
// MODULE: Elliptical Wedge Shell (v3.5)
// Front view = Ellipse, Top view = 120° Wedge
// ============================================================================

// Helper: Create elliptical cross-section at given Y position
// Width follows wedge taper, height also tapers toward back
module ellipse_slice(y_pos, inset=0) {
    // Calculate width at this Y position (follows wedge geometry)
    w = wedge_width_at(y_pos) - inset * 2;

    // Height tapers slightly toward back (ellipse becomes smaller)
    y_ratio = y_pos / DEPTH;
    h = HEIGHT * (1 - y_ratio * BACK_ELLIPSE_SCALE) - inset * 2;

    // Position ellipse at correct Y and center vertically
    translate([0, y_pos, HEIGHT/2 - (HEIGHT * y_ratio * BACK_ELLIPSE_SCALE)/2])
    rotate([90, 0, 0])
    resize([w, h, 0])
    cylinder(d=1, h=0.01, center=true, $fn=128);
}

// Outer elliptical wedge shell with lofted cross-sections
module outer_shell_elliptical(show_cutaway=false) {
    color(C_MDF_DARK, 0.35) {
        difference() {
            // === Outer surface: hull of multiple ellipses ===
            hull() {
                // Front ellipse
                ellipse_slice(WALL_THICKNESS/2);

                // Intermediate slices for smooth loft
                for (i = [1 : ELLIPSE_STEPS - 1]) {
                    y = WALL_THICKNESS + (DEPTH - WALL_THICKNESS - BACK_CORNER_RADIUS) * i / ELLIPSE_STEPS;
                    ellipse_slice(y);
                }

                // Back (near tip)
                ellipse_slice(DEPTH - BACK_CORNER_RADIUS);
            }

            // === Inner cavity (same shape, inset by wall thickness) ===
            hull() {
                // Front inner ellipse
                ellipse_slice(WALL_THICKNESS * 1.5, WALL_THICKNESS);

                // Intermediate inner slices
                for (i = [1 : ELLIPSE_STEPS - 1]) {
                    y = WALL_THICKNESS * 2 + (DEPTH - WALL_THICKNESS * 3 - BACK_CORNER_RADIUS) * i / ELLIPSE_STEPS;
                    ellipse_slice(y, WALL_THICKNESS);
                }

                // Back inner (with wall thickness offset)
                ellipse_slice(DEPTH - BACK_CORNER_RADIUS - WALL_THICKNESS, WALL_THICKNESS);
            }

            // === LCD aperture (front face, follows ellipse curve) ===
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

            // === Bottom intake vents (forward position, ellipse floor) ===
            for (x = [-100, -50, 0, 50, 100]) {
                hull() {
                    translate([x-12, DEPTH * 0.35, -1])
                    cylinder(d=5, h=WALL_THICKNESS + 2);
                    translate([x+12, DEPTH * 0.35, -1])
                    cylinder(d=5, h=WALL_THICKNESS + 2);
                }
            }

            // === Top exhaust (moved forward due to narrow back) ===
            translate([0, DEPTH * 0.4, HEIGHT - WALL_THICKNESS/2])
            cylinder(d=MIC_MESH_DIA + 15, h=WALL_THICKNESS + 1, center=true);

            // === Side connectors (on angled sides of wedge) ===
            // Left side: DC jack + USB-C
            translate([-FRONT_WIDTH/4, 0, HEIGHT * 0.35]) {
                rotate([0, 0, -30])
                translate([0, -1, 0]) {
                    // DC jack
                    translate([0, 0, 15])
                    rotate([-90, 0, 0])
                    cylinder(d=11, h=WALL_THICKNESS + 2);

                    // USB-C
                    translate([0, 0, -10])
                    rotate([-90, 0, 0])
                    hull() {
                        for (x = [-3.5, 3.5]) {
                            translate([x, 0, 0])
                            cylinder(d=4, h=WALL_THICKNESS + 2);
                        }
                    }
                }
            }

            // Right side: SMA antenna
            translate([FRONT_WIDTH/4, 0, HEIGHT * 0.35])
            rotate([0, 0, 30])
            translate([0, -1, 0])
            rotate([-90, 0, 0])
            cylinder(d=7, h=WALL_THICKNESS + 2);

            // === Cutaway for section view ===
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
// MODULE: Reinforced Bottom Plate (v3.5 Elliptical)
// ============================================================================
module reinforced_bottom_plate() {
    color(C_MDF_DARK) {
        difference() {
            // Thick bottom plate following elliptical wedge shape
            linear_extrude(height=BOTTOM_PLATE_THICK)
            elliptical_wedge_2d(inset=WALL_THICKNESS);

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
// MODULE: Reinforced Top Plate (v3.5 Elliptical)
// ============================================================================
module reinforced_top_plate() {
    translate([0, 0, HEIGHT - TOP_PLATE_THICK])
    color(C_MDF_DARK) {
        difference() {
            // Thick top plate following elliptical wedge shape (slightly inset)
            linear_extrude(height=TOP_PLATE_THICK)
            elliptical_wedge_2d(inset=WALL_THICKNESS + 2);

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
    // 8 bolt points connecting plates to shell (aligned with forward tower)
    color([0.6, 0.6, 0.65]) {
        // Bottom plate to shell bolts (corners)
        translate([0, DEPTH/2 + TOWER_Y_OFFSET, BOTTOM_PLATE_THICK/2])
        for (x = [-1, 1]) {
            for (y = [-1, 1]) {
                translate([x * (PLATE_L1[0]/2 + 15), y * (PLATE_L1[1]/2 + 15), 0])
                cylinder(d=4, h=10, center=true);
            }
        }

        // Top plate to shell bolts (corners)
        translate([0, DEPTH/2 + TOWER_Y_OFFSET, HEIGHT - TOP_PLATE_THICK/2])
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
// MODULE: Compact Shelf Plate (with I2S Cable Channels)
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

        // === I2S Cable Channels (4 corners, separated DAC/MIC) ===
        // Front-left: I2S0 DAC (BCLK=12, WS=10, DOUT=9)
        translate([-(w/2 - CABLE_DUCT_SIZE[0]/2 - 2), -(d/2 - CABLE_DUCT_SIZE[1]/2 - 2), -0.1]) {
            // Main duct
            cube([CABLE_DUCT_SIZE[0], CABLE_DUCT_SIZE[1], PLATE_THICKNESS + 0.2], center=true);
            // Separator for shielded cable
            translate([I2S_CHANNEL_W/2, 0, 0])
            cube([1, CABLE_DUCT_SIZE[1] - 2, PLATE_THICKNESS + 0.2], center=true);
        }

        // Front-right: I2S1 MIC (BCLK=45, WS=46, DIN=47)
        translate([(w/2 - CABLE_DUCT_SIZE[0]/2 - 2), -(d/2 - CABLE_DUCT_SIZE[1]/2 - 2), -0.1]) {
            cube([CABLE_DUCT_SIZE[0], CABLE_DUCT_SIZE[1], PLATE_THICKNESS + 0.2], center=true);
            translate([-I2S_CHANNEL_W/2, 0, 0])
            cube([1, CABLE_DUCT_SIZE[1] - 2, PLATE_THICKNESS + 0.2], center=true);
        }

        // Back-left: Power cables
        translate([-(w/2 - CABLE_DUCT_SIZE[0]/2 - 2), (d/2 - CABLE_DUCT_SIZE[1]/2 - 2), -0.1])
        cube([CABLE_DUCT_SIZE[0], CABLE_DUCT_SIZE[1], PLATE_THICKNESS + 0.2], center=true);

        // Back-right: I2C + GPIO cables
        translate([(w/2 - CABLE_DUCT_SIZE[0]/2 - 2), (d/2 - CABLE_DUCT_SIZE[1]/2 - 2), -0.1])
        cube([CABLE_DUCT_SIZE[0], CABLE_DUCT_SIZE[1], PLATE_THICKNESS + 0.2], center=true);

        // === Ventilation (40% open area chimney effect) ===
        if (with_vent) {
            // Elongated slots for better airflow
            for (i = [-1, 0, 1]) {
                hull() {
                    translate([i * w/4 - 6, 0, -0.1])
                    cylinder(d=5, h=PLATE_THICKNESS + 0.2);
                    translate([i * w/4 + 6, 0, -0.1])
                    cylinder(d=5, h=PLATE_THICKNESS + 0.2);
                }
            }
            // Additional cross slots
            for (j = [-1, 1]) {
                hull() {
                    translate([0, j * d/4 - 4, -0.1])
                    cylinder(d=4, h=PLATE_THICKNESS + 0.2);
                    translate([0, j * d/4 + 4, -0.1])
                    cylinder(d=4, h=PLATE_THICKNESS + 0.2);
                }
            }
        }
    }
}

// ============================================================================
// MODULE: Compact Center Tower (Moved Forward for True Wedge)
// ============================================================================
module center_tower() {
    // Tower moved forward to avoid narrow back
    translate([0, DEPTH/2 + TOWER_Y_OFFSET, 0]) {

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
// MODULE: Speaker Box Pair (Strong Toe-in for True Wedge)
// ============================================================================
module speaker_boxes() {
    // Left speaker (stronger toe-in follows wedge angle)
    translate([-SPK_X_OFFSET, SPK_Y_OFFSET, SPK_Z_OFFSET])
    rotate([0, 0, SPK_TOE_IN])
    trapezoidal_speaker_box();

    // Right speaker (mirrored with same toe-in)
    translate([SPK_X_OFFSET, SPK_Y_OFFSET, SPK_Z_OFFSET])
    rotate([0, 0, -SPK_TOE_IN])
    mirror([1, 0, 0])
    trapezoidal_speaker_box();
}

// ============================================================================
// MODULE: LCD Assembly (B&O Style with Tapered Bezel & Shadow Gap)
// ============================================================================
module lcd_assembly() {
    // Inset position (12mm from front panel for floating effect)
    translate([0, WALL_THICKNESS + LCD_Y_INSET, LCD_Z_CENTER]) {

        // === LCD Module (floating inside bezel) ===
        color(C_LCD)
        translate([0, LCD_SHADOW_GAP, 0])  // Shadow gap offset
        cube([LCD_MODULE[0], LCD_MODULE[2], LCD_MODULE[1]], center=true);

        // Active display surface
        color([0.08, 0.10, 0.15])
        translate([0, LCD_SHADOW_GAP - 2.5, 0])
        cube([LCD_ACTIVE[0], 0.5, LCD_ACTIVE[1]], center=true);

        // === B&O Style Tapered Bezel ===
        // Creates a funnel-like transition from front panel to LCD
        color(C_FRAME) {
            difference() {
                // Tapered bezel body (wider at front, narrower at LCD)
                hull() {
                    // Front opening (flush with shell front)
                    translate([0, -LCD_Y_INSET/2, 0])
                    for (x = [-1, 1]) {
                        for (z = [-1, 1]) {
                            translate([x * (LCD_BEZEL[0]/2 - 8), 0, z * (LCD_BEZEL[1]/2 - 8)])
                            rotate([90, 0, 0])
                            cylinder(r=8, h=1);
                        }
                    }

                    // Back opening (close to LCD, smaller)
                    translate([0, LCD_Y_INSET/2 - 3, 0])
                    for (x = [-1, 1]) {
                        for (z = [-1, 1]) {
                            translate([x * (LCD_MODULE[0]/2 + 3), 0, z * (LCD_MODULE[1]/2 + 3)])
                            rotate([90, 0, 0])
                            cylinder(r=3, h=1);
                        }
                    }
                }

                // Tapered inner cutout (creates the funnel)
                hull() {
                    // Front inner edge
                    translate([0, -LCD_Y_INSET/2 - 1, 0])
                    cube([LCD_BEZEL[0] - 16, 2, LCD_BEZEL[1] - 16], center=true);

                    // Back inner edge (matches LCD + shadow gap)
                    translate([0, LCD_Y_INSET/2, 0])
                    cube([LCD_MODULE[0] + 2, 2, LCD_MODULE[1] + 2], center=true);
                }
            }
        }

        // === Shadow Gap Ring (B&O floating effect) ===
        // Dark ring around LCD creates visual separation
        color([0.02, 0.02, 0.03])
        difference() {
            translate([0, LCD_SHADOW_GAP/2, 0])
            cube([LCD_MODULE[0] + 4, LCD_SHADOW_GAP + 0.5, LCD_MODULE[1] + 4], center=true);

            translate([0, LCD_SHADOW_GAP/2, 0])
            cube([LCD_MODULE[0] - 2, LCD_SHADOW_GAP + 1, LCD_MODULE[1] - 2], center=true);
        }
    }
}

// ============================================================================
// MODULE: XVF3800 (Top Mount, Moved Forward)
// ============================================================================
module xvf3800() {
    // Mic array moved forward with tower
    translate([0, DEPTH/2 + TOWER_Y_OFFSET, MIC_Z]) {
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
// MODULE: Inner Frame (Adjusted for True Wedge)
// ============================================================================
module inner_frame() {
    color(C_FRAME, 0.6) {
        // Tower base (moved forward)
        translate([0, DEPTH/2 + TOWER_Y_OFFSET, WALL_THICKNESS]) {
            difference() {
                cylinder(d=PLATE_L1[0] + 15, h=8);
                cylinder(d=PLATE_L1[0] - 8, h=9);
            }
        }

        // Speaker cradles (follow strong toe-in angle)
        for (side = [-1, 1]) {
            translate([side * SPK_X_OFFSET, SPK_Y_OFFSET + SPK_DEPTH/2, SPK_Z_OFFSET - 3])
            rotate([0, 0, side * SPK_TOE_IN])
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

        // Mic mount (moved forward with tower)
        translate([0, DEPTH/2 + TOWER_Y_OFFSET, HEIGHT - 12])
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
    outer_shell_elliptical(show_cutaway=false);  // v3.5 elliptical wedge
    monocoque_structure();      // Unified structural assembly
    center_tower();
    speaker_boxes();
    lcd_assembly();
    xvf3800();
    inner_frame();
    fabric_grille();
}

module exploded_view() {
    translate([0, 0, 0]) outer_shell_elliptical(show_cutaway=true);
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
    color(C_MDF_DARK, 0.4) outer_shell_elliptical(show_cutaway=false);
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
===================================================================
v3.5 Elliptical Front + 120° Wedge Top View
===================================================================

KEY CHANGES FROM v3.4:

1. ELLIPTICAL FRONT PROFILE:
   - Front view: Smooth elliptical shape
   - Major axis: FRONT_WIDTH (380mm horizontal)
   - Minor axis: HEIGHT (180mm vertical)
   - Creates organic, speaker-like appearance

2. 120° WEDGE TOP VIEW MAINTAINED:
   - Top view still shows 120° wedge/fan shape
   - Width tapers from FRONT_WIDTH to BACK_WIDTH
   - Back tip nearly pointed (30mm)

3. LOFTED SHELL CONSTRUCTION:
   - Uses hull() with multiple elliptical cross-sections
   - ELLIPSE_STEPS slices for smooth transition
   - Each slice follows wedge_width_at(y) function
   - Height tapers slightly toward back (BACK_ELLIPSE_SCALE)

4. INTERNAL STRUCTURE PRESERVED:
   - All internal components unchanged
   - Speaker boxes, tower, LCD remain in place
   - Plates updated to elliptical profiles

GEOMETRY:
  Front view (XZ plane):
         ╭───────────────────╮
        ╱                     ╲
       │                       │  ← Ellipse: 380mm × 180mm
       │         LCD           │
        ╲                     ╱
         ╰───────────────────╯

  Top view (XY plane):
         ┌──────────────────────┐
        ╱                        ╲
       ╱                          ╲  ← FRONT: 380mm
      │      ○ Spk    Spk ○       │
       ╲          ○ Tower        ╱
        ╲                       ╱
         ╲                    ╱
          ╲        ●         ╱   ← BACK: 30mm (120° wedge)
           ╰───────────────╯

===================================================================
v3.4 True Wedge Shape - Formation Wedge Authentic
===================================================================

KEY CHANGES FROM v3.3:

1. TRUE WEDGE GEOMETRY:
   - BACK_WIDTH: 150mm → 30mm (dramatic narrowing)
   - Creates authentic Formation Wedge triangular profile
   - Taper ratio: 1.75mm/mm (350mm over 200mm depth)

2. SMOOTH BACK CURVE:
   - Back corner radius: 30mm (vs 15mm front)
   - Single large arc at back creates teardrop profile
   - hull() with 3 circles: 2 front corners + 1 back center

3. COMPONENT REPOSITIONING:
   - Center tower: moved 25mm forward (TOWER_Y_OFFSET)
   - XVF3800 mic: follows tower forward
   - Connectors: moved to angled sides (no longer on narrow back)
   - DC jack + USB-C: left side at -30° angle
   - SMA antenna: right side at +30° angle

4. SPEAKER TOE-IN:
   - Angle: 8° → 15° (follows steeper wedge taper)
   - Speaker depth: 150mm → 120mm (fits narrow back)
   - Better acoustic imaging with strong toe-in

WEDGE PROFILE (Top View):
         ┌──────────────────────────┐
        ╱                            ╲
       ╱                              ╲  ← FRONT_WIDTH = 380mm
      │      ○ Speaker    Speaker ○    │
      │           ○ Tower ○            │  ← Tower moved forward
       ╲          ○ Mic                ╱
        ╲                            ╱
         ╲                          ╱
          ╲        ●               ╱   ← BACK_WIDTH = 30mm
           ╲______●●●_____________╱      (with R=30mm curve)

CONNECTOR PLACEMENT:
  Left side (-30°):  DC jack, USB-C
  Right side (+30°): SMA antenna
  (Back too narrow for connectors)

===================================================================
v3.3 HiFi Rose / B&O Design Philosophy:
===================================================================

KEY CHANGES FROM v3.2:

1. MONOCOQUE SHELL (HiFi Rose Style):
   - Wall thickness: 6mm → 8mm (structural rigidity)
   - Corner radius: 10mm → 15mm (Formation Wedge curves)
   - Internal reinforcement ribs: 5mm thick, 30mm high

2. SPEAKER BOX OPTIMIZATION:
   - Front width: 75mm → 105mm
   - Back width: 50mm → 65mm
   - Depth: 120mm → 150mm (user requested)
   - Height: 115mm → 145mm
   - X offset: 85mm → 68mm (centered for fitment)
   - Internal volume: ~0.95L per channel

3. B&O STYLE LCD BEZEL:
   - Inset depth: 10mm → 12mm
   - Tapered funnel bezel (wide at front, narrow at LCD)
   - 1mm shadow gap for floating effect
   - Dark ring creates visual separation

4. I2S CABLE MANAGEMENT:
   - Separated cable channels at 4 corners
   - Front-left: I2S0 DAC (BCLK=12, WS=10, DOUT=9)
   - Front-right: I2S1 MIC (BCLK=45, WS=46, DIN=47)
   - Back-left: Power cables
   - Back-right: I2C + GPIO

5. ENHANCED VENTILATION:
   - 40% open area chimney slots
   - Cross-slot pattern for better airflow

DESIGN VERIFICATION (OpenSCAD console output):
  Speaker back Y: 165mm
  Wedge inner half-width: ~96mm
  Speaker outer edge: 68 + 32.5 = 100.5mm
  Note: Speakers slightly extend - use toe-in angle to fit

VOLUME CALCULATION:
  Internal: (81+41)/2 × 126 × 121 = ~0.93L per channel
  (Suitable for 2.5" full-range + passive radiator)

ESP32-P4 PIN ASSIGNMENT (Verified in main.c):
  I2S0 (DAC): BCLK=12, WS=10, DOUT=9
  I2S1 (Mic): BCLK=45, WS=46, DIN=47
  I2C (Sensors): SDA=7, SCL=8

===================================================================
v3.2 Slim Monocoque Design (Previous):
===================================================================

KEY CHANGES FROM v3.1:
1. MDF Thickness: 19mm → 12mm
2. Speaker boxes fit inside wedge envelope
3. Monocoque bolt points

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
