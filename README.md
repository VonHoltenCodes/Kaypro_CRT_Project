# Kaypro CRT Driver for Teensy 4.1

Drive vintage 1980s Kaypro monochrome CRT monitors with modern hardware.

## Overview

This project enables you to drive a **Kaypro KP-1254G** 12-inch green phosphor CRT monitor (circa 1986) using a Teensy 4.1 microcontroller. The monitor uses IBM MDA/Hercules-compatible timing signals.

## Hardware

### Monitor Specifications
- **Model:** Kaypro KP-1254G
- **Display:** 12-inch green phosphor CRT
- **Resolution:** 720 × 350 pixels
- **Horizontal Frequency:** 18.432 kHz (54.25 µs per line)
- **Vertical Frequency:** 50 Hz (20 ms per frame)
- **Signal Levels:** TTL (5V nominal, 3.3V works)
- **Connector:** DB-9 Female

### Microcontroller
- **Board:** Teensy 4.1
- **CPU:** ARM Cortex-M7 @ 600 MHz
- **Why Teensy:** Precise hardware timers for stable sync signals

## Connections

| Teensy Pin | Monitor DB-9 Pin | Signal          |
|------------|------------------|-----------------|
| GND        | Pin 1 (or 2)     | Ground          |
| Pin 2      | Pin 7            | Video           |
| Pin 3      | Pin 6            | Intensity       |
| Pin 4      | Pin 8            | Horizontal Sync |
| Pin 5      | Pin 9            | Vertical Sync   |

**Important:** Horizontal Sync is **positive polarity**, Vertical Sync is **negative polarity**.

## Installation

### Requirements
- [PlatformIO](https://platformio.org/) (or Arduino IDE with Teensyduino)
- Teensy 4.1 board
- Kaypro KP-1254G monitor (or compatible MDA monitor)

### Build and Upload

```bash
# Using PlatformIO
pio run              # Compile
pio run --target upload   # Upload (press Teensy button when prompted)
```

### Arduino IDE Setup
1. Install Teensyduino
2. Tools → Board → Teensy 4.1
3. Tools → CPU Speed → 600 MHz
4. Tools → USB Type → Serial
5. Upload sketch

## Version History

### v1.0 - Initial Release (2026-04-05)
- Hardware timer-based horizontal sync (18.432 kHz)
- Stable vertical sync (50 Hz)
- Simple horizontal stripe test pattern
- Verified stable image lock on Kaypro CRT

## Technical Details

### Timing Implementation
Uses Teensy's `IntervalTimer` for precise horizontal line timing (54.25 µs). Software delays cannot provide the sub-microsecond precision required for CRT sync signals.

### Video Generation
Video is generated line-by-line in the timer interrupt. Currently produces simple test patterns; future versions will support:
- Text rendering
- Graphics
- Live data display
- Serial communication

## Roadmap

- [x] v1.0 - Basic sync and test patterns
- [ ] v1.1 - Convergence test patterns (crosshair, grid, circles)
- [ ] v1.2 - Bitmap font text rendering engine
- [ ] v1.3 - ASCII art displays
- [ ] v2.0 - Serial communication for live data
- [ ] v2.1 - Terminal mirror display
- [ ] v2.2 - Weather dashboard

## Contributing

This project is open for contributions! Ideas welcome:
- Support for other vintage monitors
- Additional test patterns
- Graphical capabilities
- Gaming on vintage CRTs

## License

MIT License - Feel free to use and modify!

## Credits

Developed at **NEONpulseTECHSHOP gigalab** - Human-Claude collaborative lab environment.

## Safety Notes

- Vintage CRT monitors contain high voltages even when unplugged
- Only connect signal cables while monitor is off
- Do not open the monitor case unless you know what you're doing
- Discharge CRT properly before any internal work

## Resources

- [Teensy 4.1 Documentation](https://www.pjrc.com/store/teensy41.html)
- [MDA/Hercules Timing Specs](http://www.tinyvga.com/vga-timing)
- Kaypro Monitor Service Manual: See `/docs` folder

---

**Project Status:** Active Development
**Last Updated:** 2026-04-05
