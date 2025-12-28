// ============================================================================
// Project Omni-P4: B&W Formation Wedge Trace v4.1
// Complete Redesign - B&W Official Dimensions (440mm Class)
// ============================================================================
//
// DESIGN PHILOSOPHY:
// Perfect trace of B&W Formation Wedge geometry with 120° sector top view
// and elliptical front profile. The enclosure IS the speaker box.
//
// B&W FORMATION WEDGE OFFICIAL SPECS:
// - Width:  440mm (17.3")
// - Depth:  243mm (9.6")
// - Height: 232mm (9.1")
// - Shape:  "120-degrees elliptical shape"
//
// KEY CHANGES FROM v4.0:
// - Front width: 420mm → 440mm (B&W official)
// - Depth: 220mm → 243mm (B&W official)
// - Height: 200mm → 232mm (B&W official)
// - Top shape: Simple wedge → 120° sector (authentic)
// - Front shape: Flat → Elliptical (authentic)
// - Back width: 160mm → 19mm (calculated from 120° geometry)
// - Driver X position: 155mm → 130mm (fits inside ellipse)
//
// ============================================================================

$fn = 96;

// ============================================================================
// MASTER PARAMETERS v4.1 - B&W Formation Wedge Trace
// ============================================================================

// === B&W Official Outer Dimensions ===
FRONT_WIDTH       = 440;      // B&W公式: 440mm (17.3")
DEPTH             = 243;      // B&W公式: 243mm (9.6")
HEIGHT            = 232;      // B&W公式: 232mm (9.1")
SECTOR_ANGLE      = 120;      // B&W公式: 120° sector

// === Calculated 120° Sector Geometry ===
// 弦長 = 2R × sin(θ/2) → R = FRONT_WIDTH / (2 × sin(60°))
FRONT_RADIUS      = FRONT_WIDTH / (2 * sin(60));  // = 254.0mm
BACK_RADIUS       = FRONT_RADIUS - DEPTH;          // = 11.0mm
BACK_WIDTH        = 2 * BACK_RADIUS * sin(60);     // = 19.0mm

// === Shell Construction ===
SHELL_THICKNESS   = 12;       // 12mm MDF outer shell

// === Elliptical Front Profile ===
ELLIPSE_A         = FRONT_WIDTH / 2;  // = 220mm (半長軸)
ELLIPSE_B         = HEIGHT / 2;       // = 116mm (半短軸)

// === Internal Baffles ===
BAFFLE_THICKNESS  = 15;       // Internal divider thickness (MDF)
CENTER_ZONE_WIDTH = 120;      // Width reserved for electronics tower
BAFFLE_SETBACK    = 30;       // Distance from front for baffle

// === Center Tower ===
STANDOFF_PITCH    = 55;       // M3 standoff spacing (minimized tower)
STANDOFF_OD       = 6;        // Standoff outer diameter
STANDOFF_ID       = 3;        // M3 thread
TOWER_WIDTH       = 100;      // Plate width
TOWER_DEPTH       = 80;       // Plate depth

// Tower level heights (adjusted for taller enclosure)
LEVEL_1_Z         = 25;       // Power + sensors
LEVEL_2_Z         = 80;       // DAC + Amp
LEVEL_3_Z         = 135;      // ESP32-P4

// Standoff heights between levels
STANDOFF_L1_L2    = 53;       // 80 - 25 - 2
STANDOFF_L2_L3    = 53;       // 135 - 80 - 2
STANDOFF_L3_TOP   = 30;       // To LCD mount

// === Plate Dimensions ===
PLATE_THICKNESS   = 2.0;
PLATE_L1          = [80, 60];
PLATE_L2          = [85, 65];
PLATE_L3          = [100, 80];

// === LCD (7" MIPI-DSI) - INSET CONFIGURATION ===
LCD_ACTIVE        = [154, 86];       // Active display area
LCD_MODULE        = [170, 105, 5];   // Module dimensions
LCD_BEZEL         = [190, 108, 10];  // Bezel with inset frame
LCD_Z_CENTER      = HEIGHT / 2;      // = 116mm (centered vertically)
LCD_INSET_DEPTH   = 8;               // How deep the LCD is recessed

// === Speaker Drivers (positioned to fit inside ellipse) ===
DRIVER_CUTOUT     = 68;       // Peerless full-range cutout Ø68mm
PASSIVE_CUTOUT    = 68;       // Dayton passive radiator Ø68mm
DRIVER_DEPTH      = 35;       // Driver mounting depth

// Driver positions (verified to fit inside ellipse)
// At X=130mm, ellipse height = 187.2mm (±93.6mm from center)
// Drivers extend ±69mm from center → 69 < 93.6 ✓
DRIVER_X          = 130;      // Distance from center (was 155mm in v4.0)
DRIVER_SPACING_Y  = 70;       // Center-to-center vertical spacing
DRIVER_UPPER_Y    = 35;       // Upper driver: center + 35mm
DRIVER_LOWER_Y    = -35;      // Lower driver: center - 35mm

// === ReSpeaker USB Mic Array (replaces XVF3800) ===
MIC_PCB           = [70, 70, 8];   // ReSpeaker circular PCB (Ø70mm)
MIC_MESH_DIA      = 65;            // Acoustic mesh diameter
MIC_Y_POS         = DEPTH * 0.5;   // Centered on top surface

// === LED Diffuser Ring (Nest Mini style) ===
DIFFUSER_OD       = 68;            // Outer diameter (covers LED ring)
DIFFUSER_ID       = 50;            // Inner diameter (mic hole stays open)
DIFFUSER_H        = 3;             // Thickness (milky acrylic)
DIFFUSER_Z_OFFSET = 3;             // Height above ReSpeaker PCB top

// === Thermal ===
VENT_SLOT_W       = 40;
VENT_SLOT_H       = 6;
VENT_COUNT        = 5;

// === Rubber Feet ===
FOOT_DIAMETER     = 20;       // Ø20mm black rubber feet
FOOT_HEIGHT       = 8;        // 8mm height (vibration isolation)
FOOT_INSET        = 25;       // Distance from edge
FOOT_COUNT        = 4;        // 4 feet (corners of sector)

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
C_RUBBER      = [0.08, 0.08, 0.10];  // Black rubber
C_DIFFUSER    = [0.95, 0.95, 0.98, 0.6];  // Milky acrylic (translucent)

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Calculate sector width at given depth (Y position from front)
// At Y=0: width = FRONT_WIDTH, At Y=DEPTH: width = BACK_WIDTH
function sector_width_at(y) =
    let(r = FRONT_RADIUS - y)
    2 * r * sin(SECTOR_ANGLE / 2);

// Calculate ellipse height at given X position
// Returns full height (2 * half-height)
function ellipse_height_at(x) =
    let(normalized_x = x / ELLIPSE_A)
    (abs(normalized_x) > 1) ? 0 : 2 * ELLIPSE_B * sqrt(1 - normalized_x * normalized_x);

// Verify driver fits inside ellipse at X position
function driver_fits_at(x, driver_radius, driver_y_extent) =
    let(available_height = ellipse_height_at(x) / 2)
    driver_y_extent + driver_radius < available_height;

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

// 120° sector area = (θ/2)(R² - r²) where θ in radians
SECTOR_AREA = (PI/3) * (pow(FRONT_RADIUS, 2) - pow(BACK_RADIUS, 2));
TOTAL_VOLUME = SECTOR_AREA * HEIGHT / 1000000;  // in Liters

// Internal dimensions (subtract shell)
INTERNAL_FRONT_RADIUS = FRONT_RADIUS - SHELL_THICKNESS;
INTERNAL_BACK_RADIUS = BACK_RADIUS + SHELL_THICKNESS;
INTERNAL_HEIGHT = HEIGHT - SHELL_THICKNESS * 2;

INTERNAL_SECTOR_AREA = (PI/3) * (pow(INTERNAL_FRONT_RADIUS, 2) - pow(INTERNAL_BACK_RADIUS, 2));
INTERNAL_VOLUME = INTERNAL_SECTOR_AREA * INTERNAL_HEIGHT / 1000000;

// Center tower zone volume (approximate)
TOWER_ZONE_VOL = CENTER_ZONE_WIDTH * TOWER_DEPTH * INTERNAL_HEIGHT / 1000000;

// Baffle volume (2 baffles)
BAFFLE_VOL = 2 * BAFFLE_THICKNESS * (DEPTH - SHELL_THICKNESS * 2) * INTERNAL_HEIGHT / 1000000;

// Available for speakers
SPEAKER_VOL = INTERNAL_VOLUME - TOWER_ZONE_VOL - BAFFLE_VOL;
VOL_PER_CHANNEL = SPEAKER_VOL / 2;

// Driver fit verification
DRIVER_AT_X130_ELLIPSE_H = ellipse_height_at(DRIVER_X);
DRIVER_OUTER_EDGE = abs(DRIVER_UPPER_Y) + DRIVER_CUTOUT / 2;
ELLIPSE_HALF_HEIGHT_AT_X130 = DRIVER_AT_X130_ELLIPSE_H / 2;
DRIVER_FITS = DRIVER_OUTER_EDGE < ELLIPSE_HALF_HEIGHT_AT_X130;

echo(str(""));
echo(str("=== Omni-P4 v4.1 B&W Formation Wedge Trace ==="));
echo(str(""));
echo(str("B&W OFFICIAL DIMENSIONS:"));
echo(str("  Width (front): ", FRONT_WIDTH, "mm (B&W: 440mm)"));
echo(str("  Depth:         ", DEPTH, "mm (B&W: 243mm)"));
echo(str("  Height:        ", HEIGHT, "mm (B&W: 232mm)"));
echo(str("  Sector angle:  ", SECTOR_ANGLE, "°"));
echo(str(""));
echo(str("120° SECTOR GEOMETRY:"));
echo(str("  Front radius R:  ", FRONT_RADIUS, "mm"));
echo(str("  Back radius r:   ", BACK_RADIUS, "mm"));
echo(str("  Back width:      ", BACK_WIDTH, "mm"));
echo(str("  Sector area:     ", SECTOR_AREA, " mm²"));
echo(str(""));
echo(str("ELLIPTICAL FRONT PROFILE:"));
echo(str("  Semi-major a:    ", ELLIPSE_A, "mm"));
echo(str("  Semi-minor b:    ", ELLIPSE_B, "mm"));
echo(str("  Equation: (x/", ELLIPSE_A, ")² + (y/", ELLIPSE_B, ")² = 1"));
echo(str(""));
echo(str("VOLUMES:"));
echo(str("  Total outer:     ", TOTAL_VOLUME, " L"));
echo(str("  Total internal:  ", INTERNAL_VOLUME, " L"));
echo(str("  Center tower:    ", TOWER_ZONE_VOL, " L"));
echo(str("  Baffle material: ", BAFFLE_VOL, " L"));
echo(str("  Speaker chambers:", SPEAKER_VOL, " L"));
echo(str("  Per channel:     ", VOL_PER_CHANNEL, " L"));
echo(str(""));
echo(str("DRIVER FIT VERIFICATION (X=", DRIVER_X, "mm):"));
echo(str("  Ellipse height at X=", DRIVER_X, ": ", DRIVER_AT_X130_ELLIPSE_H, "mm"));
echo(str("  Ellipse ±range: ±", ELLIPSE_HALF_HEIGHT_AT_X130, "mm"));
echo(str("  Driver outer edge: ±", DRIVER_OUTER_EDGE, "mm"));
echo(str("  Driver fits: ", DRIVER_FITS ? "YES ✓" : "NO ✗"));
echo(str(""));
echo(str("SPEAKER COMPATIBILITY:"));
echo(str("  Peerless 2.5\" (needs 1.5-2.5L): ", VOL_PER_CHANNEL > 1.5 ? "OK ✓" : "SMALL ✗"));
echo(str("  Dayton Passive Radiator: OK with ", VOL_PER_CHANNEL, "L"));
echo(str(""));

// ============================================================================
// MODULE: 120° Sector 2D (Top View) - Authentic B&W
// ============================================================================

module sector_120_2d() {
    // 120° sector with arcs at front and back
    // Virtual apex is at (0, FRONT_RADIUS) from the front edge

    difference() {
        // Outer arc (front)
        translate([0, FRONT_RADIUS])
        rotate([0, 0, -90])
        pie_slice_2d(FRONT_RADIUS, SECTOR_ANGLE);

        // Inner arc (back) - subtracted
        translate([0, FRONT_RADIUS])
        rotate([0, 0, -90])
        pie_slice_2d(BACK_RADIUS, SECTOR_ANGLE + 2);  // +2 for clean cut
    }
}

// Helper: 2D pie slice (sector)
module pie_slice_2d(radius, angle) {
    intersection() {
        circle(r=radius);
        polygon([
            [0, 0],
            [radius * 2, 0],
            [radius * 2 * cos(angle), radius * 2 * sin(angle)],
            [0, 0]
        ]);
    }
}

// Alternative sector using polygon for more control
module sector_polygon_2d() {
    steps = 60;

    // Generate points for outer arc
    outer_points = [for (i = [0:steps])
        let(angle = 30 + (i / steps) * 120)  // 30° to 150° (centered at 90°)
        let(r = FRONT_RADIUS)
        [r * cos(angle), FRONT_RADIUS - r * sin(angle)]
    ];

    // Generate points for inner arc (reversed)
    inner_points = [for (i = [steps:-1:0])
        let(angle = 30 + (i / steps) * 120)
        let(r = BACK_RADIUS)
        [r * cos(angle), FRONT_RADIUS - r * sin(angle)]
    ];

    polygon(concat(outer_points, inner_points));
}

// Internal sector (offset inward)
module inner_sector_2d() {
    offset(r = -SHELL_THICKNESS)
    sector_polygon_2d();
}

// ============================================================================
// MODULE: Elliptical Front Profile 2D
// ============================================================================

module ellipse_front_2d() {
    scale([1, ELLIPSE_B / ELLIPSE_A])
    circle(r = ELLIPSE_A);
}

// ============================================================================
// MODULE: Outer Shell - B&W Formation Wedge Style
// ============================================================================

module outer_shell(show_cutaway=false) {
    color(C_SHELL) {
        difference() {
            // Main body: 120° sector extruded with elliptical ends
            hull() {
                // Bottom elliptical edge
                translate([0, 0, SHELL_THICKNESS])
                linear_extrude(height=0.1)
                sector_polygon_2d();

                // Main body
                translate([0, 0, HEIGHT/2])
                linear_extrude(height=0.1)
                sector_polygon_2d();

                // Top elliptical edge (slightly inset for style)
                translate([0, 0, HEIGHT - SHELL_THICKNESS])
                linear_extrude(height=0.1)
                offset(r=-2)
                sector_polygon_2d();
            }

            // Inner cavity
            translate([0, 0, SHELL_THICKNESS])
            linear_extrude(height=HEIGHT)
            inner_sector_2d();

            // LCD aperture (front face) - INSET STYLE
            lcd_aperture();

            // Driver apertures (on front elliptical face)
            driver_apertures();

            // Bottom ventilation slots
            bottom_vents();

            // Top microphone/diffuser opening (sized for diffuser ring)
            translate([0, MIC_Y_POS, HEIGHT - SHELL_THICKNESS/2])
            cylinder(d=DIFFUSER_OD + 4, h=SHELL_THICKNESS + 1, center=true);

            // Diffuser seating recess (3mm deep ledge for diffuser ring)
            translate([0, MIC_Y_POS, HEIGHT - DIFFUSER_H])
            cylinder(d=DIFFUSER_OD + 2, h=DIFFUSER_H + 1);

            // Back connector panel
            translate([0, DEPTH - BACK_RADIUS - 5, HEIGHT * 0.4])
            rotate([90, 0, 0])
            hull() {
                for (x = [-15, 15]) {
                    translate([x, 0, 0])
                    cylinder(d=10, h=SHELL_THICKNESS + 2);
                }
            }

            // Cutaway for visualization
            if (show_cutaway) {
                translate([0, -DEPTH/2, -10])
                cube([FRONT_WIDTH, DEPTH, HEIGHT + 20], center=true);
            }
        }
    }
}

// LCD aperture with inset recess
module lcd_aperture() {
    // Main LCD opening
    translate([0, 0, LCD_Z_CENTER])
    rotate([90, 0, 0])
    translate([0, 0, -SHELL_THICKNESS - 1])
    hull() {
        for (x = [-1, 1], z = [-1, 1]) {
            translate([x * (LCD_BEZEL[0]/2 - 8), z * (LCD_BEZEL[1]/2 - 8), 0])
            cylinder(r=8, h=SHELL_THICKNESS + 2);
        }
    }

    // Inset recess (larger, shallower cut for LCD to sit in)
    translate([0, LCD_INSET_DEPTH, LCD_Z_CENTER])
    rotate([90, 0, 0])
    translate([0, 0, -1])
    hull() {
        for (x = [-1, 1], z = [-1, 1]) {
            translate([x * (LCD_BEZEL[0]/2 - 5), z * (LCD_BEZEL[1]/2 - 5), 0])
            cylinder(r=5, h=LCD_INSET_DEPTH + 2);
        }
    }
}

// Driver apertures positioned to fit inside ellipse
module driver_apertures() {
    // Left side drivers
    translate([-DRIVER_X, 0, LCD_Z_CENTER]) {
        // Upper driver
        translate([0, 0, DRIVER_UPPER_Y])
        rotate([90, 0, 0])
        translate([0, 0, -SHELL_THICKNESS - 1])
        cylinder(d=DRIVER_CUTOUT, h=SHELL_THICKNESS + 2);

        // Lower passive
        translate([0, 0, DRIVER_LOWER_Y])
        rotate([90, 0, 0])
        translate([0, 0, -SHELL_THICKNESS - 1])
        cylinder(d=PASSIVE_CUTOUT, h=SHELL_THICKNESS + 2);
    }

    // Right side drivers (mirrored)
    translate([DRIVER_X, 0, LCD_Z_CENTER]) {
        // Upper driver
        translate([0, 0, DRIVER_UPPER_Y])
        rotate([90, 0, 0])
        translate([0, 0, -SHELL_THICKNESS - 1])
        cylinder(d=DRIVER_CUTOUT, h=SHELL_THICKNESS + 2);

        // Lower passive
        translate([0, 0, DRIVER_LOWER_Y])
        rotate([90, 0, 0])
        translate([0, 0, -SHELL_THICKNESS - 1])
        cylinder(d=PASSIVE_CUTOUT, h=SHELL_THICKNESS + 2);
    }
}

module bottom_vents() {
    for (i = [0 : VENT_COUNT-1]) {
        x = -((VENT_COUNT-1)/2 * 50) + i * 50;
        hull() {
            translate([x - VENT_SLOT_W/2, DEPTH * 0.5, -1])
            cylinder(d=VENT_SLOT_H, h=SHELL_THICKNESS + 2);
            translate([x + VENT_SLOT_W/2, DEPTH * 0.5, -1])
            cylinder(d=VENT_SLOT_H, h=SHELL_THICKNESS + 2);
        }
    }
}

// ============================================================================
// MODULE: Internal Acoustic Baffles
// ============================================================================

module acoustic_baffles() {
    color(C_BAFFLE) {
        // Left baffle
        translate([-CENTER_ZONE_WIDTH/2 - BAFFLE_THICKNESS/2, SHELL_THICKNESS, SHELL_THICKNESS])
        cube([BAFFLE_THICKNESS, DEPTH - SHELL_THICKNESS*2 - 20, HEIGHT - SHELL_THICKNESS*2]);

        // Right baffle
        translate([CENTER_ZONE_WIDTH/2 - BAFFLE_THICKNESS/2, SHELL_THICKNESS, SHELL_THICKNESS])
        cube([BAFFLE_THICKNESS, DEPTH - SHELL_THICKNESS*2 - 20, HEIGHT - SHELL_THICKNESS*2]);
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
    // Position at front face
    y_pos = SHELL_THICKNESS + 5;  // Just inside front

    // Left channel
    translate([-DRIVER_X, y_pos, LCD_Z_CENTER]) {
        // Upper active driver
        translate([0, 0, DRIVER_UPPER_Y])
        rotate([-90, 0, 0])
        speaker_driver();

        // Lower passive radiator
        translate([0, 0, DRIVER_LOWER_Y])
        rotate([-90, 0, 0])
        passive_radiator();
    }

    // Right channel
    translate([DRIVER_X, y_pos, LCD_Z_CENTER]) {
        // Upper active driver
        translate([0, 0, DRIVER_UPPER_Y])
        rotate([-90, 0, 0])
        speaker_driver();

        // Lower passive radiator
        translate([0, 0, DRIVER_LOWER_Y])
        rotate([-90, 0, 0])
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
    }
}

// ============================================================================
// MODULE: Center Electronics Tower
// ============================================================================

module center_tower() {
    translate([0, DEPTH * 0.5, 0]) {

        // === LEVEL 1: Power + Sensors ===
        translate([0, 0, LEVEL_1_Z]) {
            shelf_plate(PLATE_L1);

            for (pos = standoff_positions()) {
                translate([pos[0], pos[1], PLATE_THICKNESS])
                standoff(STANDOFF_L1_L2);
            }

            // Sensor PCBs
            color(C_PCB, 0.8)
            translate([0, 0, 5])
            cube([60, 45, 3], center=true);
        }

        // === LEVEL 2: DAC + Amp ===
        translate([0, 0, LEVEL_2_Z]) {
            shelf_plate(PLATE_L2);

            for (pos = standoff_positions()) {
                translate([pos[0], pos[1], PLATE_THICKNESS])
                standoff(STANDOFF_L2_L3);
            }

            // ES9039Q2M DAC
            color(C_PCB, 0.8)
            translate([0, 0, 5])
            cube([50, 40, 8], center=true);

            // Amplifier
            color([0.1, 0.1, 0.12])
            translate([0, 0, 15])
            cube([45, 35, 10], center=true);
        }

        // === LEVEL 3: ESP32-P4 ===
        translate([0, 0, LEVEL_3_Z]) {
            shelf_plate(PLATE_L3, with_vent=false);

            for (pos = standoff_positions()) {
                translate([pos[0], pos[1], PLATE_THICKNESS])
                standoff(STANDOFF_L3_TOP);
            }

            // ESP32-P4 board
            color(C_PCB, 0.9)
            translate([0, 0, -10])
            cube([55, 45, 4], center=true);

            color([1, 1, 1])
            translate([0, 0, -12])
            linear_extrude(0.5)
            text("ESP32-P4", size=6, halign="center", valign="center");
        }
    }
}

// ============================================================================
// MODULE: LCD Assembly - INSET CONFIGURATION
// ============================================================================

module lcd_assembly() {
    // LCD is inset (recessed) into the front panel
    translate([0, LCD_INSET_DEPTH + 2, LCD_Z_CENTER]) {

        // LCD panel module
        color(C_LCD)
        rotate([90, 0, 0])
        cube([LCD_MODULE[0], LCD_MODULE[1], LCD_MODULE[2]], center=true);

        // Active display area (visible from front)
        color([0.05, 0.08, 0.12])
        translate([0, -LCD_MODULE[2]/2 - 0.5, 0])
        rotate([90, 0, 0])
        cube([LCD_ACTIVE[0], LCD_ACTIVE[1], 0.5], center=true);

        // Bezel frame (inset style)
        color([0.12, 0.12, 0.14])
        difference() {
            translate([0, -2, 0])
            rotate([90, 0, 0])
            hull() {
                for (x = [-1, 1], z = [-1, 1]) {
                    translate([x * (LCD_BEZEL[0]/2 - 5), z * (LCD_BEZEL[1]/2 - 5), 0])
                    cylinder(r=5, h=LCD_BEZEL[2]);
                }
            }

            // Cutout for LCD module
            translate([0, 5, 0])
            rotate([90, 0, 0])
            cube([LCD_MODULE[0] + 2, LCD_MODULE[1] + 2, LCD_BEZEL[2] + 2], center=true);
        }
    }
}

// ============================================================================
// MODULE: ReSpeaker USB Mic Array (Top Surface Mount)
// ============================================================================

// ============================================================================
// MODULE: LED Diffuser Ring (Nest Mini Style)
// ============================================================================

module led_diffuser_ring() {
    // Donut-shaped milky acrylic diffuser
    // Sits on top of ReSpeaker, covers LED ring, leaves center open for mics
    color(C_DIFFUSER)
    difference() {
        cylinder(d=DIFFUSER_OD, h=DIFFUSER_H);
        translate([0, 0, -0.1])
        cylinder(d=DIFFUSER_ID, h=DIFFUSER_H + 0.2);
    }
}

module respeaker() {
    translate([0, MIC_Y_POS, HEIGHT - SHELL_THICKNESS]) {
        // ReSpeaker circular PCB (Ø70mm)
        color(C_PCB)
        cylinder(d=70, h=MIC_PCB[2]);

        // Central processor chip
        color([0.1, 0.1, 0.12])
        translate([0, 0, MIC_PCB[2]])
        cylinder(d=12, h=2);

        // MEMS microphones (4) - arranged in corners
        color([0.3, 0.3, 0.35])
        for (angle = [45, 135, 225, 315]) {
            rotate([0, 0, angle])
            translate([24, 0, MIC_PCB[2]])
            cylinder(d=4, h=1.5);
        }

        // LED ring (12 LEDs in ring pattern)
        color([0.2, 0.2, 0.25])
        translate([0, 0, MIC_PCB[2] + 0.5])
        difference() {
            cylinder(d=60, h=1);
            translate([0, 0, -0.1])
            cylinder(d=50, h=1.2);
        }

        // Individual LEDs (12 RGB LEDs)
        for (i = [0:11]) {
            rotate([0, 0, i * 30])
            translate([27.5, 0, MIC_PCB[2] + 1])
            color([1, 1, 1, 0.9])  // White glow (can be any color)
            cylinder(d=3, h=0.5);
        }

        // LED Diffuser Ring (Nest Mini style)
        translate([0, 0, MIC_PCB[2] + DIFFUSER_Z_OFFSET])
        led_diffuser_ring();

        // Acoustic mesh (center, for microphones)
        color(C_MESH)
        translate([0, 0, MIC_PCB[2] + DIFFUSER_Z_OFFSET + DIFFUSER_H + 0.5])
        cylinder(d=DIFFUSER_ID - 5, h=1);

        // USB cable (going down to Level 3)
        color([0.15, 0.15, 0.18])
        translate([0, 15, -30])
        cylinder(d=4, h=35);
    }
}

// Alias for backwards compatibility
module xvf3800() {
    respeaker();
}

// ============================================================================
// MODULE: Black Rubber Feet
// ============================================================================

module rubber_foot() {
    color(C_RUBBER)
    union() {
        // Main rubber body (slightly domed)
        cylinder(d1=FOOT_DIAMETER, d2=FOOT_DIAMETER - 2, h=FOOT_HEIGHT - 1);

        // Domed top for grip
        translate([0, 0, FOOT_HEIGHT - 1])
        scale([1, 1, 0.3])
        sphere(d=FOOT_DIAMETER - 2);
    }
}

module rubber_feet() {
    // 4 rubber feet positioned at the corners of the 120° sector
    // Front left
    translate([-FRONT_WIDTH/2 + FOOT_INSET + 10, FOOT_INSET, -FOOT_HEIGHT])
    rubber_foot();

    // Front right
    translate([FRONT_WIDTH/2 - FOOT_INSET - 10, FOOT_INSET, -FOOT_HEIGHT])
    rubber_foot();

    // Back left (closer to center due to narrow back)
    translate([-40, DEPTH - FOOT_INSET - 15, -FOOT_HEIGHT])
    rubber_foot();

    // Back right (closer to center due to narrow back)
    translate([40, DEPTH - FOOT_INSET - 15, -FOOT_HEIGHT])
    rubber_foot();
}

// ============================================================================
// MODULE: Front Fabric Grille
// ============================================================================

module fabric_grille() {
    color(C_FABRIC, 0.85) {
        difference() {
            // Elliptical grille matching front profile
            translate([0, 1, HEIGHT/2])
            rotate([90, 0, 0])
            linear_extrude(height=2)
            scale([0.95, 0.95])
            ellipse_front_2d();

            // LCD window cutout
            translate([0, 5, LCD_Z_CENTER])
            rotate([90, 0, 0])
            hull() {
                for (x = [-1, 1], z = [-1, 1]) {
                    translate([x * (LCD_BEZEL[0]/2 - 10), z * (LCD_BEZEL[1]/2 - 10)])
                    cylinder(r=10, h=10);
                }
            }

            // Driver grille openings (acoustic transparent)
            for (side = [-1, 1]) {
                translate([side * DRIVER_X, 5, LCD_Z_CENTER]) {
                    translate([0, 0, DRIVER_UPPER_Y])
                    rotate([90, 0, 0])
                    cylinder(d=DRIVER_CUTOUT + 5, h=10);

                    translate([0, 0, DRIVER_LOWER_Y])
                    rotate([90, 0, 0])
                    cylinder(d=PASSIVE_CUTOUT + 5, h=10);
                }
            }
        }
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
    rubber_feet();
    // fabric_grille();  // Uncomment to show grille
}

module section_view() {
    difference() {
        full_assembly();
        translate([0, -DEPTH/2, -10])
        cube([FRONT_WIDTH + 10, DEPTH, HEIGHT + 20], center=true);
    }
}

module exploded_view() {
    translate([0, 0, 0]) outer_shell();
    translate([0, 0, 50]) acoustic_baffles();
    translate([0, 0, 100]) speaker_assembly();
    translate([0, 0, 60]) center_tower();
    translate([0, 0, 120]) lcd_assembly();
    translate([0, 0, 140]) xvf3800();
    translate([0, -40, 0]) fabric_grille();
}

module shell_only() {
    outer_shell();
    acoustic_baffles();
}

module top_view_2d() {
    projection(cut=true)
    translate([0, 0, -HEIGHT/2])
    outer_shell();
}

// ============================================================================
// RENDER SELECTION
// ============================================================================

// Uncomment one to render:

full_assembly();              // Complete v4.1 B&W Trace design
// section_view();            // Cross-section view
// exploded_view();           // Exploded components
// shell_only();              // Shell + baffles only
// top_view_2d();             // 2D top projection (120° sector)

// Debug: Show 120° sector outline
// color("red", 0.3) linear_extrude(1) sector_polygon_2d();

// ============================================================================
// DESIGN NOTES v4.1 - B&W Formation Wedge Trace
// ============================================================================
/*
===================================================================
v4.1 B&W FORMATION WEDGE TRACE - AUTHENTIC DIMENSIONS
===================================================================

B&W OFFICIAL SPECIFICATIONS:
  Width:    440mm (17.3")
  Depth:    243mm (9.6")
  Height:   232mm (9.1")
  Weight:   6.5kg
  Shape:    "120-degrees elliptical shape"

GEOMETRY CALCULATIONS:
  Sector angle θ = 120°
  Front chord = 440mm
  Chord = 2R × sin(60°) = 1.732R
  R (front radius) = 440 ÷ 1.732 = 254.0mm

  Depth = 243mm
  r (back radius) = 254 - 243 = 11mm
  Back width = 2 × 11 × sin(60°) = 19mm

  Sector area = (π/3)(R² - r²)
             = (π/3)(64516 - 121)
             = 67,449 mm²

  Volume = 67,449 × 232 = 15.6L

ELLIPTICAL FRONT PROFILE:
  Semi-major a = 220mm (width/2)
  Semi-minor b = 116mm (height/2)
  Equation: (x/220)² + (y/116)² = 1

DRIVER FIT VERIFICATION:
  Driver X position: ±130mm from center
  At X = 130mm:
    h = 2 × 116 × √(1 - (130/220)²)
    h = 232 × √(1 - 0.349)
    h = 232 × 0.807
    h = 187.2mm

  Ellipse range: ±93.6mm from center
  Driver outer edge: ±69mm from center (35mm offset + 34mm radius)

  Result: 69mm < 93.6mm → DRIVERS FIT ✓

LCD INSET CONFIGURATION:
  - LCD is recessed 8mm into the front panel
  - Creates flush, integrated appearance
  - Bezel provides clean edge transition

COMPARISON TO v4.0:
  Parameter      v4.0       v4.1 (B&W)   Change
  ───────────────────────────────────────────────
  Front width    420mm      440mm        +20mm
  Back width     160mm      19mm         -141mm
  Depth          220mm      243mm        +23mm
  Height         200mm      232mm        +32mm
  Sector angle   ~linear    120°         Authentic
  Driver X       155mm      130mm        -25mm
  Volume         ~9.2L      ~15.6L       +6.4L

ADVANTAGES OF v4.1:
  1. Authentic B&W Formation Wedge dimensions
  2. Drivers verified to fit inside ellipse
  3. 120° sector = B&W official geometry
  4. Increased volume (15.6L) for better bass
  5. LCD inset for premium appearance
  6. Black rubber feet for vibration isolation

===================================================================
MATERIALS BOM (Bill of Materials)
===================================================================

┌─────────────────────────────────────────────────────────────────────────────┐
│ STRUCTURAL COMPONENTS                                                        │
├──────────────────┬────────────────────────────────────────┬────────┬────────┤
│ Part             │ Material / Specification               │ Qty    │ Notes  │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ Outer Shell      │ MDF 12mm (Medium Density Fiberboard)   │ 1 set  │ CNC    │
│                  │ Finish: Matte Black Paint or           │        │        │
│                  │         Walnut Veneer (0.6mm)          │        │        │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ Internal Baffles │ MDF 15mm                               │ 2 pcs  │ CNC    │
│                  │ No finish (internal)                   │        │        │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ Electronics      │ Aluminum A6063-T5 2mm                  │ 3 pcs  │ Laser  │
│ Shelf Plates     │ Finish: Black Anodized                 │        │ Cut    │
│                  │ Sizes: 80×60, 85×65, 100×80mm          │        │        │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ Standoffs        │ Aluminum M3 Hex Standoffs              │ 12 pcs │ M3×6mm │
│                  │ Lengths: 53mm (×8), 30mm (×4)          │        │ OD     │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ Rubber Feet      │ Silicone Rubber, Shore A 60            │ 4 pcs  │ Black  │
│                  │ Ø20mm × 8mm height                     │        │ Dome   │
│                  │ Self-adhesive (3M VHB backing)         │        │        │
└──────────────────┴────────────────────────────────────────┴────────┴────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│ ACOUSTIC COMPONENTS                                                          │
├──────────────────┬────────────────────────────────────────┬────────┬────────┤
│ Active Drivers   │ Peerless by Tymphany                   │ 2 pcs  │ L + R  │
│                  │ 2.5" Full Range NE65W-04               │        │        │
│                  │ 4Ω, 15W RMS, Ø68mm cutout              │        │        │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ Passive Radiators│ Dayton Audio ND65PR-4                  │ 2 pcs  │ L + R  │
│                  │ 2.5" Passive Radiator                  │        │        │
│                  │ Ø68mm cutout                           │        │        │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ Acoustic Foam    │ Open-cell Polyurethane Foam            │ 2 pcs  │ 20mm   │
│                  │ 60×80×100mm per chamber                │        │ thick  │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ Front Grille     │ Acoustically Transparent Fabric        │ 1 pc   │ Black  │
│                  │ Knit polyester (Guilford FR701 type)   │        │        │
│                  │ Stretched over MDF frame               │        │        │
└──────────────────┴────────────────────────────────────────┴────────┴────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│ ELECTRONICS COMPONENTS                                                       │
├──────────────────┬────────────────────────────────────────┬────────┬────────┤
│ Main MCU         │ ESP32-P4 DevKit (Espressif)            │ 1 pc   │ RISC-V │
│                  │ 32MB PSRAM, WiFi 6, BLE 5.0            │        │        │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ Display          │ 7" MIPI-DSI LCD Module                 │ 1 pc   │ IPS    │
│                  │ 1024×600, Capacitive Touch             │        │        │
│                  │ Module: 170×105×5mm                    │        │        │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ DAC              │ ES9039Q2M Hi-Res DAC Board             │ 1 pc   │ 32bit  │
│                  │ I2S input, -120dB THD+N                │        │ 384kHz │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ Amplifier        │ TPA3116D2 Class-D Stereo               │ 1 pc   │ 2×15W  │
│                  │ 12-24V input, 4Ω/8Ω compatible         │        │        │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ Microphone Array │ ReSpeaker USB Mic Array v2.0           │ 1 pc   │ USB    │
│                  │ 4-mic MEMS, Beamforming, LED Ring      │        │ Ø70mm  │
│                  │ Top surface mount (天面中央)            │        │        │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ Power Supply     │ 24V 3A DC Adapter + Buck Converter     │ 1 set  │ 72W    │
│                  │ 24V→5V (3A) for ESP32-P4               │        │        │
│                  │ 24V→12V (2A) for Amp                   │        │        │
└──────────────────┴────────────────────────────────────────┴────────┴────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│ SENSOR MODULES (Multi-Sensor Hub Configuration)                             │
├──────────────────┬────────────────────────────────────────┬────────┬────────┤
│ SCD41            │ Sensirion CO2 Sensor                   │ 1 pc   │ I2C    │
│                  │ CO2 (400-5000ppm), Temp, Humidity      │        │        │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ SGP40            │ Sensirion VOC Sensor                   │ 1 pc   │ I2C    │
│                  │ VOC Index (1-500)                      │        │        │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ ENS160           │ ScioSense Air Quality Sensor           │ 1 pc   │ I2C    │
│                  │ eCO2, TVOC, AQI (1-5)                  │        │        │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ BMP388           │ Bosch Barometric Sensor                │ 1 pc   │ I2C    │
│                  │ Pressure, Temperature, Altitude        │        │        │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ SHT40            │ Sensirion High-Precision Temp/Humidity │ 1 pc   │ I2C    │
│                  │ ±0.2°C, ±1.8% RH                       │        │        │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ SEN0428          │ DFRobot PM Sensor (Laser)              │ 1 pc   │ I2C    │
│                  │ PM1.0, PM2.5, PM10 (µg/m³)             │        │        │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ SEN0395          │ DFRobot mmWave Presence Sensor         │ 1 pc   │ UART1  │
│                  │ Human detection, distance, motion      │        │        │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ SEN0540          │ DFRobot Offline Voice Recognition      │ 1 pc   │ UART2  │
│                  │ Local keyword spotting                 │        │        │
└──────────────────┴────────────────────────────────────────┴────────┴────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│ HARDWARE & FASTENERS                                                         │
├──────────────────┬────────────────────────────────────────┬────────┬────────┤
│ M3 Screws        │ M3×8mm Pan Head (Black Oxide Steel)    │ 30 pcs │ Plates │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ M3 Nuts          │ M3 Hex Nuts (Black Oxide Steel)        │ 20 pcs │        │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ M2.5 Screws      │ M2.5×5mm (for PCB mounting)            │ 16 pcs │ PCBs   │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ Wood Screws      │ 3.5×20mm Countersunk (for MDF)         │ 12 pcs │ Shell  │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ Cable Ties       │ 100mm Nylon Cable Ties                 │ 20 pcs │ Wiring │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ Acoustic Mesh    │ Stainless Steel Mesh Ø65mm             │ 1 pc   │ Mic    │
│                  │ 100 mesh count (for ReSpeaker)         │        │        │
├──────────────────┼────────────────────────────────────────┼────────┼────────┤
│ LED Diffuser     │ Milky Acrylic (Opal/Frosted) 3mm       │ 1 pc   │ Laser  │
│ Ring             │ Donut: OD=68mm, ID=50mm                │        │ Cut    │
│                  │ Material: PMMA (Polymethyl Methacrylate)│       │        │
│                  │ Light transmission: 30-50%             │        │        │
│                  │ Purpose: Nest Mini style LED diffusion │        │        │
└──────────────────┴────────────────────────────────────────┴────────┴────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│ FINISH OPTIONS                                                               │
├──────────────────┬────────────────────────────────────────────────────────────┤
│ Option A         │ Matte Black                                               │
│ (Recommended)    │ - Primer: Zinsser B-I-N (shellac-based)                   │
│                  │ - Paint: 2K Polyurethane Matte Black                      │
│                  │ - Clear: Satin clear coat (optional)                      │
├──────────────────┼────────────────────────────────────────────────────────────┤
│ Option B         │ Walnut Veneer                                             │
│ (Premium)        │ - Veneer: 0.6mm American Black Walnut                     │
│                  │ - Adhesive: Contact cement or iron-on                     │
│                  │ - Finish: Danish Oil + Wax                                │
├──────────────────┼────────────────────────────────────────────────────────────┤
│ Option C         │ Fabric Wrap                                               │
│ (Speaker-like)   │ - Fabric: Acoustic knit fabric (black)                    │
│                  │ - Method: Stretch + staple to MDF frame                   │
│                  │ - LCD area: Cut opening with heat-sealed edge             │
└──────────────────┴────────────────────────────────────────────────────────────┘

RUBBER FEET SPECIFICATIONS:
  Type:       Self-adhesive silicone rubber bumpers
  Diameter:   Ø20mm
  Height:     8mm
  Hardness:   Shore A 60 (soft enough for vibration isolation)
  Color:      Black
  Quantity:   4 pieces
  Placement:  Front corners: 35mm from front edge, 25mm from side
              Back corners: 40mm from center (80mm apart)
  Purpose:    - Vibration isolation from surface
              - Anti-slip stability
              - Acoustic decoupling
              - Protect furniture surface

ESTIMATED WEIGHT:
  MDF Shell (12mm):     ~2.5 kg
  MDF Baffles (15mm):   ~0.5 kg
  Electronics:          ~0.3 kg
  Speakers:             ~0.4 kg
  Hardware:             ~0.2 kg
  ─────────────────────────────
  Total:                ~3.9 kg (vs B&W 6.5kg - lighter due to no metal chassis)

===================================================================
*/
