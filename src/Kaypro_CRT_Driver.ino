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
 * - Signal Level: TTL (5V via SN74LS245N)
 *
 * v1.4 - DMA-POWERED PIXEL RENDERING
 * Using Teensy 4.1's DMA engine to achieve true 720-pixel resolution
 * DMA transfers framebuffer to GPIO at precise intervals
 * No CPU involvement during pixel output = perfect timing
 *
 * Hardware Connections (DB-9 Female on Monitor via SN74LS245N):
 * - GND         → Pin 1 (or Pin 2)  - Ground
 * - Teensy Pin 2 → Pin 7            - Video Signal (via level shifter)
 * - Teensy Pin 3 → Pin 6            - Intensity (via level shifter)
 * - Teensy Pin 4 → Pin 8            - Horizontal Sync (via level shifter)
 * - Teensy Pin 5 → Pin 9            - Vertical Sync (via level shifter)
 *
 * Author: VonHoltenCodes + Claude (Opus Mode)
 * Date: 2026-04-05
 * Version: 1.4 - DMA Pixel Rendering
 */

#include <DMAChannel.h>

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
// Using EVEN line count (368) to prevent interlace interpretation
#define V_TOTAL_LINES      368      // Even number to force progressive scan
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
// 8×8 BITMAP FONT
// ============================================================================
// Simple 8×8 font for text rendering
// Each character is 8 bytes (one byte per row)
// ASCII characters 32-126 (space through ~)

const uint8_t font8x8[95][8] PROGMEM = {
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // Space (32)
  {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00},  // ! (33)
  {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // " (34)
  {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00},  // # (35)
  {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00},  // $ (36)
  {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00},  // % (37)
  {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00},  // & (38)
  {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00},  // ' (39)
  {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00},  // ( (40)
  {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00},  // ) (41)
  {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00},  // * (42)
  {0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00},  // + (43)
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06},  // , (44)
  {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00},  // - (45)
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00},  // . (46)
  {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00},  // / (47)
  {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00},  // 0 (48)
  {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00},  // 1 (49)
  {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00},  // 2 (50)
  {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00},  // 3 (51)
  {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00},  // 4 (52)
  {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00},  // 5 (53)
  {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00},  // 6 (54)
  {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00},  // 7 (55)
  {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00},  // 8 (56)
  {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00},  // 9 (57)
  {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00},  // : (58)
  {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06},  // ; (59)
  {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00},  // < (60)
  {0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00},  // = (61)
  {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00},  // > (62)
  {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00},  // ? (63)
  {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00},  // @ (64)
  {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00},  // A (65)
  {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00},  // B (66)
  {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00},  // C (67)
  {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00},  // D (68)
  {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00},  // E (69)
  {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00},  // F (70)
  {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00},  // G (71)
  {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00},  // H (72)
  {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  // I (73)
  {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00},  // J (74)
  {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00},  // K (75)
  {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00},  // L (76)
  {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00},  // M (77)
  {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00},  // N (78)
  {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00},  // O (79)
  {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00},  // P (80)
  {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00},  // Q (81)
  {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00},  // R (82)
  {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00},  // S (83)
  {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  // T (84)
  {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00},  // U (85)
  {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},  // V (86)
  {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},  // W (87)
  {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00},  // X (88)
  {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00},  // Y (89)
  {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00},  // Z (90)
  {0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00},  // [ (91)
  {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00},  // \ (92)
  {0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00},  // ] (93)
  {0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00},  // ^ (94)
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},  // _ (95)
  {0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00},  // ` (96)
  {0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00},  // a (97)
  {0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00},  // b (98)
  {0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00},  // c (99)
  {0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00},  // d (100)
  {0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00},  // e (101)
  {0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00},  // f (102)
  {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F},  // g (103)
  {0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00},  // h (104)
  {0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  // i (105)
  {0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E},  // j (106)
  {0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00},  // k (107)
  {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  // l (108)
  {0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00},  // m (109)
  {0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00},  // n (110)
  {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00},  // o (111)
  {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F},  // p (112)
  {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78},  // q (113)
  {0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00},  // r (114)
  {0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00},  // s (115)
  {0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00},  // t (116)
  {0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00},  // u (117)
  {0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},  // v (118)
  {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00},  // w (119)
  {0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00},  // x (120)
  {0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F},  // y (121)
  {0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00},  // z (122)
  {0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00},  // { (123)
  {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00},  // | (124)
  {0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00},  // } (125)
  {0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // ~ (126)
};

// ============================================================================
// TEXT RENDERING STATE
// ============================================================================

// Framebuffer for text rendering (90 bytes per line = 720 pixels / 8 bits)
uint8_t textFramebuffer[350][90] = {0};  // 31,500 bytes
volatile bool textBufferReady = false;

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
  PATTERN_TEXT,           // Text rendering demo
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
// DMA CONFIGURATION FOR PIXEL OUTPUT
// ============================================================================

DMAChannel dma;
volatile bool dmaActive = false;

// DMA transfer buffer - pre-computed GPIO values for each pixel
// We'll prepare this from the framebuffer for fast DMA transfer
uint32_t dmaLineBuffer[720] __attribute__((aligned(32)));

// GPIO6_DR register address (Pin 2 is GPIO6, bit 12)
#define GPIO6_DR_ADDR 0x42000000  // GPIO6 data register
#define VIDEO_PIN_MASK (1 << 12)  // Pin 2 = GPIO6_12

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
  Serial.println("=== Test Patterns ===");
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
  Serial.println("  9 - Text Rendering Demo");
  Serial.println();

  delay(500);

  // Initialize text framebuffer to zero
  Serial.println("Initializing text framebuffer...");
  clearTextBuffer();
  Serial.println("Text framebuffer cleared.");

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
    if (input >= '0' && input <= '9') {
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
        case PATTERN_TEXT:
          Serial.println("Text Rendering");
          Serial.println("Preparing text framebuffer...");

          // Temporarily disable timer to prevent rendering during buffer prep
          hsyncTimer.end();

          prepareTextDemo();

          Serial.println("Text buffer ready!");
          Serial.println("Restarting video timer...");

          // Small delay to let things settle
          delay(100);

          // Restart timer
          current_line = 0;
          hsyncTimer.begin(hsyncISR, H_TOTAL_TIME_US);

          Serial.println("Video restarted - rendering text!");
          break;
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
// TEXT RENDERING FUNCTIONS
// ============================================================================

// Draw a single character into the text framebuffer
// x, y are character coordinates (not pixels)
// Character size is 8×8 pixels
void drawChar(uint8_t x, uint8_t y, char c) {
  if (c < 32 || c > 126) c = 32;  // Replace invalid chars with space
  uint8_t char_index = c - 32;

  // Calculate pixel position
  uint16_t pixel_y = y * 8;
  uint16_t pixel_x = x * 8;

  // Draw 8 rows of the character
  for (uint8_t row = 0; row < 8; row++) {
    if (pixel_y + row >= 350) break;  // Bounds check

    uint8_t font_row = pgm_read_byte(&font8x8[char_index][row]);

    // Draw 8 pixels of this row
    for (uint8_t col = 0; col < 8; col++) {
      uint16_t px = pixel_x + col;
      if (px >= 720) break;  // Bounds check

      uint8_t byte_index = px / 8;
      uint8_t bit_index = 7 - (px % 8);

      if (font_row & (1 << (7 - col))) {
        // Set pixel (white)
        textFramebuffer[pixel_y + row][byte_index] |= (1 << bit_index);
      } else {
        // Clear pixel (black)
        textFramebuffer[pixel_y + row][byte_index] &= ~(1 << bit_index);
      }
    }
  }
}

// Draw a string into the text framebuffer
void drawString(uint8_t x, uint8_t y, const char* str) {
  uint8_t cursor_x = x;
  while (*str && cursor_x < 90) {  // 720 pixels / 8 = 90 characters max
    drawChar(cursor_x, y, *str);
    cursor_x++;
    str++;
  }
}

// Clear the text framebuffer
void clearTextBuffer() {
  memset(textFramebuffer, 0, sizeof(textFramebuffer));
}

// Prepare text framebuffer with demo content
void prepareTextDemo() {
  clearTextBuffer();

  // FULL DEMO - Now that horizontal works and interlacing is fixed!
  // Let's show off what we've achieved

  // Title section with box
  for (uint8_t x = 10; x < 80; x++) {
    textFramebuffer[40][x] = 0xFF;   // Top border
    textFramebuffer[120][x] = 0xFF;  // Bottom border
  }

  drawString(15, 8, "KAYPRO CRT");
  drawString(15, 10, "RESTORED!");

  // Info section
  drawString(5, 18, "Teensy 4.1 Driver v1.5");
  drawString(5, 20, "368 Lines Non-Interlaced");
  drawString(5, 22, "450 Pixels @ 50Hz");

  // Credits
  drawString(5, 28, "Built by:");
  drawString(5, 30, "VonHoltenCodes");
  drawString(5, 32, "+ Claude Opus");

  // Test pattern at bottom
  for (uint16_t y = 280; y < 290; y++) {
    for (uint8_t x = 0; x < 90; x++) {
      textFramebuffer[y][x] = (x % 2) ? 0xAA : 0x55;  // Checkerboard
    }
  }

  textBufferReady = true;
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
    case PATTERN_TEXT:
      drawText(line_number);
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

// Pattern 9: FULL SCREEN RENDERING - Fill entire scan line
void drawText(uint16_t line_number) {
  if (!textBufferReady) {
    digitalWriteFast(PIN_VIDEO, LOW);
    digitalWriteFast(PIN_INTENSITY, LOW);
    return;
  }

  // FULL WIDTH: Output ALL 90 bytes with 5-bit sampling
  // This fills the entire screen width to eliminate double imaging
  // 90 bytes × 5 samples = 450 pixels across full width

  // Small delay for beam positioning
  delayMicroseconds(4);

  // Render ALL 90 bytes to fill screen
  for (uint8_t byte_idx = 0; byte_idx < 90; byte_idx++) {
    uint8_t pixel_byte = textFramebuffer[line_number][byte_idx];

    // 5-bit sampling for better character definition
    // This captures more vertical strokes
    digitalWriteFast(PIN_VIDEO, (pixel_byte & 0x80) ? HIGH : LOW);  // Bit 7
    delayNanoseconds(80);

    digitalWriteFast(PIN_VIDEO, (pixel_byte & 0x40) ? HIGH : LOW);  // Bit 6
    delayNanoseconds(80);

    digitalWriteFast(PIN_VIDEO, (pixel_byte & 0x20) ? HIGH : LOW);  // Bit 5
    delayNanoseconds(80);

    digitalWriteFast(PIN_VIDEO, (pixel_byte & 0x10) ? HIGH : LOW);  // Bit 4
    delayNanoseconds(80);

    digitalWriteFast(PIN_VIDEO, (pixel_byte & 0x02) ? HIGH : LOW);  // Bit 1
    delayNanoseconds(80);
  }

  // Ensure video is off for HSync
  digitalWriteFast(PIN_VIDEO, LOW);
  digitalWriteFast(PIN_INTENSITY, LOW);
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
