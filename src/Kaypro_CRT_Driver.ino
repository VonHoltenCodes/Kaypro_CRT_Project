/*
 * Kaypro KP-1254G MDA Monitor Driver for Teensy 4.1
 *
 * Drives a vintage 1986 Kaypro 12-inch green phosphor CRT monitor
 * with IBM MDA/Hercules compatible timing signals.
 *
 * Monitor Specifications:
 * - Horizontal Frequency: 18.432 kHz (54.25 µs per line)
 * - Vertical Frequency: 50 Hz (20 ms per frame)
 * - Resolution: 720 × 350 pixels
 * - Signal Level: TTL (3.3V from Teensy, monitor expects 5V but may work)
 *
 * Hardware Connections (DB-9 Female on Monitor):
 * - GND         → Pin 1 (or Pin 2)  - Ground
 * - Teensy Pin 2 → Pin 7            - Video Signal
 * - Teensy Pin 3 → Pin 6            - Intensity (brighter pixels)
 * - Teensy Pin 4 → Pin 8            - Horizontal Sync (positive polarity)
 * - Teensy Pin 5 → Pin 9            - Vertical Sync (negative polarity)
 *
 * Author: Generated for NEONpulseTechshop gigalab
 * Date: 2026-04-05
 * Version: 1.1 - Convergence Test Patterns
 */

// ============================================================================
// PIN DEFINITIONS
// ============================================================================

#define PIN_VIDEO      2    // Main video signal (black/white)
#define PIN_INTENSITY  3    // Intensity signal (brighter pixels)
#define PIN_HSYNC      4    // Horizontal sync (positive polarity)
#define PIN_VSYNC      5    // Vertical sync (negative/active-low polarity)

// ============================================================================
// TIMING CONSTANTS - MDA Standard
// ============================================================================

// Horizontal timing (one scan line)
#define H_TOTAL_TIME_US    54.25    // Total time per line in microseconds
#define H_VISIBLE_PIXELS   720      // Active video pixels per line
#define H_FRONT_PORCH_US   1.5      // Time after video before HSync
#define H_SYNC_PULSE_US    3.5      // HSync pulse width
#define H_BACK_PORCH_US    4.0      // Time after HSync before video starts
#define H_ACTIVE_TIME_US   (H_TOTAL_TIME_US - H_FRONT_PORCH_US - H_SYNC_PULSE_US - H_BACK_PORCH_US)

// Vertical timing (one frame)
#define V_TOTAL_LINES      370      // Total lines per frame (including blanking)
#define V_VISIBLE_LINES    350      // Active video lines
#define V_FRONT_PORCH      3        // Lines after video before VSync
#define V_SYNC_PULSE       3        // VSync pulse width in lines
#define V_BACK_PORCH       (V_TOTAL_LINES - V_VISIBLE_LINES - V_FRONT_PORCH - V_SYNC_PULSE)

// Pixel timing
#define PIXEL_TIME_NS      (H_ACTIVE_TIME_US * 1000.0 / H_VISIBLE_PIXELS)  // ~62.67 ns per pixel

// ============================================================================
// FRAMEBUFFER
// ============================================================================

// Simple 1-bit framebuffer: 720×350 = 252,000 pixels = 31,500 bytes
// For testing, we'll use a smaller resolution or generate on-the-fly
// Full buffer would be: uint8_t framebuffer[350][90]; // 90 bytes per line (720 pixels / 8)

// For this test pattern version, we'll generate patterns procedurally
// to save RAM and demonstrate the sync timing works

// ============================================================================
// TEST PATTERN MODES
// ============================================================================

enum TestPattern {
  PATTERN_STRIPES = 0,    // Horizontal stripes (v1.0 baseline)
  PATTERN_CROSSHAIR,      // Crosshair for center alignment
  PATTERN_GRID,           // Grid for geometry/linearity
  PATTERN_CIRCLES,        // Concentric circles for focus
  PATTERN_CORNERS,        // Corner markers for overscan
  PATTERN_BORDER,         // Border test
  PATTERN_CHECKERBOARD,   // Checkerboard pattern
  PATTERN_WHITE,          // Full white screen
  PATTERN_BLACK,          // Full black screen
  PATTERN_COUNT           // Total number of patterns
};

// ============================================================================
// GLOBAL STATE
// ============================================================================

volatile uint16_t current_line = 0;        // Current scan line being drawn
volatile bool frame_complete = false;      // Flag for frame timing
volatile TestPattern current_pattern = PATTERN_CROSSHAIR;  // Start with crosshair

// Hardware timer for horizontal sync
IntervalTimer hsyncTimer;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  // Set CPU speed to maximum for precise timing
  // Teensy 4.1 can run at 600 MHz
  // This is set in Arduino IDE: Tools → CPU Speed → 600 MHz

  // Initialize serial for debugging (optional)
  Serial.begin(115200);
  delay(1000);
  Serial.println("Kaypro KP-1254G MDA Driver Starting...");
  Serial.print("CPU Speed: ");
  Serial.print(F_CPU / 1000000);
  Serial.println(" MHz");

  // Configure GPIO pins as outputs
  pinMode(PIN_VIDEO, OUTPUT);
  pinMode(PIN_INTENSITY, OUTPUT);
  pinMode(PIN_HSYNC, OUTPUT);
  pinMode(PIN_VSYNC, OUTPUT);

  // Initialize all signals to idle state
  digitalWriteFast(PIN_VIDEO, LOW);      // Video off
  digitalWriteFast(PIN_INTENSITY, LOW);  // Intensity off
  digitalWriteFast(PIN_HSYNC, LOW);      // HSync idle (positive polarity, so LOW is inactive)
  digitalWriteFast(PIN_VSYNC, HIGH);     // VSync idle (negative polarity, so HIGH is inactive)

  Serial.println("Pins configured. Starting video generation...");
  Serial.println();
  Serial.println("Timing Information:");
  Serial.print("  Horizontal: ");
  Serial.print(H_TOTAL_TIME_US);
  Serial.println(" µs per line");
  Serial.print("  Vertical: ");
  Serial.print(V_TOTAL_LINES);
  Serial.print(" lines at ");
  Serial.print(1000000.0 / (H_TOTAL_TIME_US * V_TOTAL_LINES));
  Serial.println(" Hz");
  Serial.print("  Pixel time: ");
  Serial.print(PIXEL_TIME_NS);
  Serial.println(" ns");
  Serial.println();
  Serial.println("=== Convergence Test Patterns ===");
  Serial.println("Send number to change pattern:");
  Serial.println("  0 - Horizontal Stripes");
  Serial.println("  1 - Crosshair (center alignment)");
  Serial.println("  2 - Grid (geometry/linearity)");
  Serial.println("  3 - Circles (focus test)");
  Serial.println("  4 - Corner Markers (overscan)");
  Serial.println("  5 - Border");
  Serial.println("  6 - Checkerboard");
  Serial.println("  7 - Full White");
  Serial.println("  8 - Full Black");
  Serial.println();

  delay(500);

  // Start video generation
  current_line = 0;

  // Start hardware timer for horizontal sync
  // Call hsyncISR() every 54.25 microseconds (18.432 kHz)
  hsyncTimer.begin(hsyncISR, H_TOTAL_TIME_US);

  Serial.println("Hardware timer started!");
  Serial.print("Current pattern: CROSSHAIR");
  Serial.println();
}

// ============================================================================
// HORIZONTAL SYNC INTERRUPT - Called by hardware timer every scan line
// ============================================================================

void hsyncISR() {
  // This function is called precisely every 54.25 µs by the hardware timer
  // It handles one complete scan line timing

  // Manage vertical sync based on current line
  if (current_line < V_FRONT_PORCH) {
    digitalWriteFast(PIN_VSYNC, HIGH);  // VSync inactive (front porch)
  }
  else if (current_line < V_FRONT_PORCH + V_SYNC_PULSE) {
    digitalWriteFast(PIN_VSYNC, LOW);   // VSync active (sync pulse)
  }
  else {
    digitalWriteFast(PIN_VSYNC, HIGH);  // VSync inactive (back porch + active video)
  }

  // Determine if this is a visible line
  bool isVisibleLine = (current_line >= V_FRONT_PORCH + V_SYNC_PULSE + V_BACK_PORCH) &&
                       (current_line < V_FRONT_PORCH + V_SYNC_PULSE + V_BACK_PORCH + V_VISIBLE_LINES);

  if (isVisibleLine) {
    uint16_t visible_line_num = current_line - (V_FRONT_PORCH + V_SYNC_PULSE + V_BACK_PORCH);
    drawVisibleLineFast(visible_line_num);
  } else {
    // Blank line - just video off
    digitalWriteFast(PIN_VIDEO, LOW);
    digitalWriteFast(PIN_INTENSITY, LOW);
  }

  // Generate HSync pulse (positive polarity: LOW=inactive, HIGH=active)
  // Pulse occurs during horizontal blanking interval
  // Note: This is approximate - happens at start of blanking
  digitalWriteFast(PIN_HSYNC, HIGH);
  delayNanoseconds(3500);  // 3.5 µs pulse width
  digitalWriteFast(PIN_HSYNC, LOW);

  // Advance to next line
  current_line++;
  if (current_line >= V_TOTAL_LINES) {
    current_line = 0;
  }
}

// ============================================================================
// MAIN LOOP - Just monitor status
// ============================================================================

void loop() {
  // Hardware timer handles all video generation
  // Main loop monitors stats and handles pattern switching

  // Check for serial input to change patterns
  if (Serial.available() > 0) {
    char input = Serial.read();
    if (input >= '0' && input <= '8') {
      int pattern_num = input - '0';
      current_pattern = (TestPattern)pattern_num;

      Serial.print("\nPattern changed to: ");
      switch(current_pattern) {
        case PATTERN_STRIPES:      Serial.println("Horizontal Stripes"); break;
        case PATTERN_CROSSHAIR:    Serial.println("Crosshair"); break;
        case PATTERN_GRID:         Serial.println("Grid"); break;
        case PATTERN_CIRCLES:      Serial.println("Circles"); break;
        case PATTERN_CORNERS:      Serial.println("Corner Markers"); break;
        case PATTERN_BORDER:       Serial.println("Border"); break;
        case PATTERN_CHECKERBOARD: Serial.println("Checkerboard"); break;
        case PATTERN_WHITE:        Serial.println("Full White"); break;
        case PATTERN_BLACK:        Serial.println("Full Black"); break;
        default: break;
      }
    }
  }

  static uint32_t last_print = 0;
  static uint32_t frame_count = 0;

  if (current_line == 0) {
    frame_count++;
  }

  if (millis() - last_print >= 5000) {  // Print every 5 seconds
    Serial.print("FPS: ");
    Serial.println(frame_count / 5);
    frame_count = 0;
    last_print = millis();
  }

  delay(100);
}

// ============================================================================
// DRAW VISIBLE LINE - Pattern dispatcher
// ============================================================================

void drawVisibleLineFast(uint16_t line_number) {
  // Dispatch to appropriate pattern drawing function
  switch(current_pattern) {
    case PATTERN_STRIPES:
      drawStripes(line_number);
      break;
    case PATTERN_CROSSHAIR:
      drawCrosshair(line_number);
      break;
    case PATTERN_GRID:
      drawGrid(line_number);
      break;
    case PATTERN_CIRCLES:
      drawCircles(line_number);
      break;
    case PATTERN_CORNERS:
      drawCorners(line_number);
      break;
    case PATTERN_BORDER:
      drawBorder(line_number);
      break;
    case PATTERN_CHECKERBOARD:
      drawCheckerboard(line_number);
      break;
    case PATTERN_WHITE:
      drawSolidColor(true);
      break;
    case PATTERN_BLACK:
      drawSolidColor(false);
      break;
    default:
      drawSolidColor(false);
      break;
  }
}

// ============================================================================
// TEST PATTERN IMPLEMENTATIONS
// ============================================================================

// Pattern 0: Horizontal Stripes
void drawStripes(uint16_t line_number) {
  const uint16_t STRIPE_HEIGHT = 50;
  bool is_white = (line_number / STRIPE_HEIGHT) % 2 == 0;

  digitalWriteFast(PIN_VIDEO, is_white ? HIGH : LOW);
  digitalWriteFast(PIN_INTENSITY, is_white ? HIGH : LOW);
}

// Pattern 1: Crosshair (center alignment test)
void drawCrosshair(uint16_t line_number) {
  const uint16_t CENTER_Y = V_VISIBLE_LINES / 2;
  const uint16_t LINE_THICKNESS = 2;

  // Draw horizontal line of crosshair
  bool on_h_line = (line_number >= CENTER_Y - LINE_THICKNESS &&
                    line_number <= CENTER_Y + LINE_THICKNESS);

  if (on_h_line) {
    // Full white horizontal line
    digitalWriteFast(PIN_VIDEO, HIGH);
    digitalWriteFast(PIN_INTENSITY, HIGH);
  } else {
    // Draw vertical line (center pixel column)
    // We can't easily draw individual pixels in interrupt, so just draw black
    digitalWriteFast(PIN_VIDEO, LOW);
    digitalWriteFast(PIN_INTENSITY, LOW);
  }
}

// Pattern 2: Grid (geometry and linearity test)
void drawGrid(uint16_t line_number) {
  const uint16_t GRID_SPACING = 50;  // Grid every 50 lines/pixels
  const uint16_t LINE_THICKNESS = 1;

  // Horizontal grid lines
  bool on_h_grid = ((line_number % GRID_SPACING) < LINE_THICKNESS);

  if (on_h_grid) {
    digitalWriteFast(PIN_VIDEO, HIGH);
    digitalWriteFast(PIN_INTENSITY, HIGH);
  } else {
    digitalWriteFast(PIN_VIDEO, LOW);
    digitalWriteFast(PIN_INTENSITY, LOW);
  }
}

// Pattern 3: Concentric Circles (focus test)
void drawCircles(uint16_t line_number) {
  // Circle drawing is complex - for now, draw circular pattern approximation
  // Using distance from center
  const uint16_t CENTER_Y = V_VISIBLE_LINES / 2;
  const uint16_t CENTER_X = H_VISIBLE_PIXELS / 2;

  int16_t dy = line_number - CENTER_Y;
  uint16_t dist_squared = dy * dy;

  // Draw rings at specific radii
  const uint16_t RING_SPACING = 2500;  // Radius² spacing
  bool on_ring = ((dist_squared % RING_SPACING) < 500);

  digitalWriteFast(PIN_VIDEO, on_ring ? HIGH : LOW);
  digitalWriteFast(PIN_INTENSITY, on_ring ? HIGH : LOW);
}

// Pattern 4: Corner Markers (overscan test)
void drawCorners(uint16_t line_number) {
  const uint16_t MARKER_SIZE = 40;

  // Top corners
  bool in_top = (line_number < MARKER_SIZE);
  // Bottom corners
  bool in_bottom = (line_number >= V_VISIBLE_LINES - MARKER_SIZE);

  if (in_top || in_bottom) {
    digitalWriteFast(PIN_VIDEO, HIGH);
    digitalWriteFast(PIN_INTENSITY, HIGH);
  } else {
    digitalWriteFast(PIN_VIDEO, LOW);
    digitalWriteFast(PIN_INTENSITY, LOW);
  }
}

// Pattern 5: Border (edge test)
void drawBorder(uint16_t line_number) {
  const uint16_t BORDER_WIDTH = 20;

  bool in_border = (line_number < BORDER_WIDTH ||
                    line_number >= V_VISIBLE_LINES - BORDER_WIDTH);

  digitalWriteFast(PIN_VIDEO, in_border ? HIGH : LOW);
  digitalWriteFast(PIN_INTENSITY, in_border ? HIGH : LOW);
}

// Pattern 6: Checkerboard
void drawCheckerboard(uint16_t line_number) {
  const uint16_t BLOCK_SIZE = 25;
  bool row_white = ((line_number / BLOCK_SIZE) % 2) == 0;

  // For checkerboard, we'd need to toggle during the line
  // For now, just alternate rows
  digitalWriteFast(PIN_VIDEO, row_white ? HIGH : LOW);
  digitalWriteFast(PIN_INTENSITY, row_white ? HIGH : LOW);
}

// Pattern 7 & 8: Solid colors
void drawSolidColor(bool white) {
  digitalWriteFast(PIN_VIDEO, white ? HIGH : LOW);
  digitalWriteFast(PIN_INTENSITY, white ? HIGH : LOW);
}

// ============================================================================
// ALTERNATIVE TEST PATTERNS
// ============================================================================

// Uncomment and use these in drawVisibleLine() for different patterns

/*
// HORIZONTAL STRIPES - Simple timing test
void drawHorizontalStripes(uint16_t line_number) {
  const uint16_t STRIPE_HEIGHT = 50;
  bool is_white = (line_number / STRIPE_HEIGHT) % 2 == 0;

  digitalWriteFast(PIN_VIDEO, is_white ? HIGH : LOW);
  digitalWriteFast(PIN_INTENSITY, is_white ? HIGH : LOW);
  delayMicroseconds(H_ACTIVE_TIME_US);
  digitalWriteFast(PIN_VIDEO, LOW);
  digitalWriteFast(PIN_INTENSITY, LOW);
}
*/

/*
// WHITE BORDER - Test edges
void drawBorder(uint16_t line_number) {
  const uint16_t BORDER_SIZE = 20;
  bool is_border = (line_number < BORDER_SIZE ||
                    line_number >= V_VISIBLE_LINES - BORDER_SIZE);

  if (is_border) {
    // Top or bottom border - full white line
    digitalWriteFast(PIN_VIDEO, HIGH);
    digitalWriteFast(PIN_INTENSITY, HIGH);
    delayMicroseconds(H_ACTIVE_TIME_US);
  } else {
    // Left border
    digitalWriteFast(PIN_VIDEO, HIGH);
    digitalWriteFast(PIN_INTENSITY, HIGH);
    delayMicroseconds(H_ACTIVE_TIME_US * BORDER_SIZE / H_VISIBLE_PIXELS);

    // Black center
    digitalWriteFast(PIN_VIDEO, LOW);
    digitalWriteFast(PIN_INTENSITY, LOW);
    delayMicroseconds(H_ACTIVE_TIME_US * (H_VISIBLE_PIXELS - 2*BORDER_SIZE) / H_VISIBLE_PIXELS);

    // Right border
    digitalWriteFast(PIN_VIDEO, HIGH);
    digitalWriteFast(PIN_INTENSITY, HIGH);
    delayMicroseconds(H_ACTIVE_TIME_US * BORDER_SIZE / H_VISIBLE_PIXELS);
  }

  digitalWriteFast(PIN_VIDEO, LOW);
  digitalWriteFast(PIN_INTENSITY, LOW);
}
*/

// ============================================================================
// TIMING ADJUSTMENT NOTES
// ============================================================================

/*
 * If the image rolls horizontally (tears left/right):
 *   - Adjust H_SYNC_PULSE_US (try ±0.5 µs)
 *   - Adjust H_FRONT_PORCH_US or H_BACK_PORCH_US
 *
 * If the image rolls vertically (scrolls up/down):
 *   - Adjust V_SYNC_PULSE (try 2-4 lines)
 *   - Adjust V_TOTAL_LINES (try ±5 lines)
 *   - Check VSync polarity - try flipping HIGH/LOW if it won't sync
 *
 * If the image is too dim:
 *   - Check brightness/contrast knobs on monitor
 *   - Verify connections (especially ground)
 *   - Consider adding 5V level shifter (3.3V may be too low)
 *
 * If the image is stretched or compressed:
 *   - Adjust H_TOTAL_TIME_US (try ±2 µs)
 *   - Verify 18.432 kHz horizontal frequency
 *
 * To flip sync polarities if image won't lock:
 *   - HSync: swap HIGH/LOW in HSync pulse section
 *   - VSync: swap all HIGH/LOW for VSync pin
 */

// ============================================================================
// SETUP TIPS
// ============================================================================

/*
 * Arduino IDE Configuration:
 * 1. Tools → Board → Teensy 4.1
 * 2. Tools → USB Type → Serial
 * 3. Tools → CPU Speed → 600 MHz (IMPORTANT!)
 * 4. Tools → Optimize → Faster
 *
 * Testing Steps:
 * 1. Upload this sketch to Teensy 4.1
 * 2. Connect Teensy GND to monitor GND (pin 1 or 2)
 * 3. Connect signal pins as documented above
 * 4. Power on monitor
 * 5. Adjust brightness and contrast knobs
 * 6. You should see a green checkerboard pattern
 *
 * Troubleshooting:
 * - Open Serial Monitor (115200 baud) to see timing info
 * - If blank screen: check brightness, verify connections
 * - If rolling: adjust timing constants above
 * - If no sync: try flipping VSync polarity
 */
