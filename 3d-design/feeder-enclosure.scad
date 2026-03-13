/**
 * Smart Pet Feeder - 3D Enclosure Design
 * OpenSCAD script
 * 
 * Features:
 * - Motor mount for SG90 servo
 * - Ultrasonic sensor mount
 * - ESP32 microcontroller placement
 * - Bowl housing
 * - Removable lid
 * - Modular design for basic/advanced modes
 */

// ============== PARAMETERS ==============
// Adjust these to tune the design
FEEDER_WIDTH = 120;    // mm
FEEDER_DEPTH = 120;    // mm
FEEDER_HEIGHT = 180;   // mm
WALL_THICKNESS = 3;    // mm
BOWL_DIAMETER = 80;    // mm
BOWL_DEPTH = 30;       // mm

// ============== MOTOR MOUNT ==============
module motor_mount() {
  // Servo mount plate
  translate([0, 0, 0]) {
    difference() {
      // Main mount block
      cube([45, 35, 10], center = true);
      
      // Screw holes (M3)
      translate([-15, -12, 5]) cylinder(r = 1.5, h = 10, center = true);
      translate([15, -12, 5]) cylinder(r = 1.5, h = 10, center = true);
      translate([-15, 12, 5]) cylinder(r = 1.5, h = 10, center = true);
      translate([15, 12, 5]) cylinder(r = 1.5, h = 10, center = true);
      
      // Servo shaft hole
      translate([0, 0, 5]) cylinder(r = 6, h = 10, center = true);
    }
  }
  
  // Dispenser chute
  translate([0, 25, -10]) {
    difference() {
      cube([30, 40, 40], center = true);
      cube([24, 34, 50], center = true);
    }
  }
}

// ============== ULTRASONIC SENSOR MOUNT ==============
module sensor_mount() {
  // Sensor housing
  difference() {
    cube([20, 15, 10], center = true);
    // Sensor cutout
    translate([0, 0, 5]) cube([16, 11, 10], center = true);
  }
  
  // Mounting tabs
  translate([-8, -10, 0]) cube([3, 5, 3]);
  translate([8, -10, 0]) cube([3, 5, 3]);
  translate([-8, 10, 0]) cube([3, 5, 3]);
  translate([8, 10, 0]) cube([3, 5, 3]);
}

// ============== ESP32 MOUNT ==============
module esp32_mount() {
  // Base plate
  difference() {
    cube([55, 35, 3], center = true);
    
    // Mounting holes
    translate([-22, -14, 0]) cylinder(r = 1.5, h = 5, center = true);
    translate([22, -14, 0]) cylinder(r = 1.5, h = 5, center = true);
    translate([-22, 14, 0]) cylinder(r = 1.5, h = 5, center = true);
    translate([22, 14, 0]) cylinder(r = 1.5, h = 5, center = true);
  }
}

// ============== FOOD BOWL ==============
module food_bowl() {
  // Bowl outer
  difference() {
    cylinder(r = BOWL_DIAMETER / 2, h = BOWL_DEPTH, center = true);
    // Inner cavity
    cylinder(r = BOWL_DIAMETER / 2 - WALL_THICKNESS, h = BOWL_DEPTH + 1, center = true);
  }
}

// ============== MAIN ENCLOSURE ==============
module main_enclosure() {
  difference() {
    // Outer shell
    cube([FEEDER_WIDTH, FEEDER_DEPTH, FEEDER_HEIGHT], center = true);
    
    // Inner cavity
    translate([0, 0, WALL_THICKNESS]) {
      cube([
        FEEDER_WIDTH - 2 * WALL_THICKNESS,
        FEEDER_DEPTH - 2 * WALL_THICKNESS,
        FEEDER_HEIGHT - WALL_THICKNESS
      ], center = true);
    }
    
    // Lid opening (top)
    translate([0, 0, FEEDER_HEIGHT / 2 - 5]) {
      cube([FEEDER_WIDTH - 10, FEEDER_DEPTH - 10, 10], center = true);
    }
    
    // Bowl opening (front)
    translate([0, FEEDER_DEPTH / 2 - WALL_THICKNESS, 0]) {
      cube([BOWL_DIAMETER + 10, 10, BOWL_DEPTH + 20], center = true);
    }
    
    // Ventilation holes
    for (i = [-20, 0, 20]) {
      for (j = [-30, -10, 10, 30]) {
        translate([i, -FEEDER_DEPTH / 2 + WALL_THICKNESS / 2, j]) {
          rotate([90, 0, 0]) cylinder(r = 2, h = WALL_THICKNESS + 1);
        }
      }
    }
    
    // Cable grommets
    translate([-40, FEEDER_DEPTH / 2, -40]) cylinder(r = 5, h = WALL_THICKNESS + 1);
    translate([40, FEEDER_DEPTH / 2, -40]) cylinder(r = 5, h = WALL_THICKNESS + 1);
  }
}

// ============== REMOVABLE LID ==============
module removable_lid() {
  // Lid plate
  difference() {
    cube([FEEDER_WIDTH - 8, FEEDER_DEPTH - 8, WALL_THICKNESS], center = true);
    
    // Handle cutout
    translate([0, 0, 0]) cube([30, 10, WALL_THICKNESS + 2], center = true);
  }
  
  // Handle
  translate([0, 0, WALL_THICKNESS / 2 + 3]) {
    difference() {
      cube([30, 15, 6], center = true);
      cube([24, 9, 10], center = true);
    }
  }
}

// ============== ASSEMBLY VIEW ==============
module assembly() {
  // Main enclosure
  color([0.8, 0.8, 0.8]) main_enclosure();
  
  // Lid
  color([0.7, 0.7, 0.7]) translate([0, 0, FEEDER_HEIGHT / 2 + WALL_THICKNESS / 2]) {
    removable_lid();
  }
  
  // Motor mount (positioned in upper section)
  color([0.3, 0.3, 0.3]) translate([0, -FEEDER_DEPTH / 4, FEEDER_HEIGHT / 4]) {
    motor_mount();
  }
  
  // ESP32 mount (positioned in upper back)
  color([0.2, 0.2, 0.5]) translate([0, -FEEDER_DEPTH / 4 + 30, 0]) {
    esp32_mount();
  }
  
  // Sensor mount (positioned in upper front)
  color([0.2, 0.2, 0.2]) translate([0, FEEDER_DEPTH / 4 - 10, FEEDER_HEIGHT / 4]) {
    sensor_mount();
  }
}

// Render the assembly
assembly();

// ============== PRINT-FRIENDLY SEPARATE PARTS ==============
// Uncomment to render individual parts:

//* translate([0, 80, 0]) motor_mount();
//* translate([80, 80, 0]) sensor_mount();
//* translate([0, 0, 0]) esp32_mount();
//* translate([80, 0, 0]) removable_lid();

// ============== DIMENSIONS FOR ASSEMBLY ==============
echo("Feeder Dimensions:");
echo(str("Width: ", FEEDER_WIDTH, "mm"));
echo(str("Depth: ", FEEDER_DEPTH, "mm"));
echo(str("Height: ", FEEDER_HEIGHT, "mm"));
echo(str("Bowl Diameter: ", BOWL_DIAMETER, "mm"));
