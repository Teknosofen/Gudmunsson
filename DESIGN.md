# Gudmunsson LED - Design Description

## Overview

A WiFi-controlled LED frame illuminator. A single addressable RGB LED lights a plexiglass frame, and the user selects the colour via a phone or laptop connected to the device's own WiFi access point. The chosen colour persists across power cycles.

---

## Hardware

### Components

| Component | Part | Notes |
|---|---|---|
| Microcontroller | Seeed XIAO ESP32-C3 | RISC-V, built-in WiFi/BLE, USB-C |
| LED | WS2818 NeoPixel (single) | Addressable RGB, 5 V tolerant data line |
| Power supply | 5 V USB battery pack (1000 mAh) | Connected via USB-C to the XIAO |

### Wiring

```
XIAO ESP32-C3          WS2818 NeoPixel
+--------------+       +--------+
|          D10 |------→| DIN    |
|          3V3 |------→| VCC    |
|          GND |------→| GND    |
+--------------+       +--------+
```

- **Data pin:** D10 (GPIO10) on the XIAO ESP32-C3
- No level shifter is required for a single LED on a short wire
- A 100 uF capacitor across VCC/GND near the LED is recommended for voltage spike protection

### Power Notes

- The ESP32-C3 WiFi radio draws ~200 mA peak during transmission
- The hardware brownout detector is disabled in firmware to prevent reset loops on battery power where brief voltage dips can occur during WiFi startup
- A 2-second stabilisation delay is inserted between LED initialisation and WiFi startup

---

## Software

### Build Environment

| Item | Value |
|---|---|
| Framework | Arduino |
| Build system | PlatformIO |
| Platform | espressif32 |
| Board target | `seeed_xiao_esp32c3` |

### Dependencies

| Library | Purpose |
|---|---|
| Adafruit NeoPixel ^1.12.0 | WS2818 LED driver |
| EEPROM (built-in) | Persistent colour storage |
| WiFi (built-in) | Access point |
| WebServer (built-in) | HTTP server |

### Architecture

```
+------------------+       +------------------+       +---------+
| Phone / Laptop   |  WiFi | ESP32-C3         |  D10  | WS2818  |
| Browser          |◄-----►| Web Server :80   |------→| NeoPixel|
+------------------+       +------------------+       +---------+
                                   |
                              EEPROM (4 bytes)
```

### Startup Sequence

1. Disable brownout detector
2. Initialise serial (115200 baud)
3. Read saved colour from EEPROM (or use default warm orange: R=255, G=150, B=0)
4. Initialise NeoPixel and apply colour
5. Wait 2 seconds for power rail stabilisation
6. Clear stale WiFi state and start Access Point (SSID: `GudmunssonLED`, open/no password)
7. Start HTTP web server on port 80

### EEPROM Layout

| Address | Content |
|---|---|
| 0 | Validity marker (`0xAA`) |
| 1 | Red (0-255) |
| 2 | Green (0-255) |
| 3 | Blue (0-255) |

On boot, if address 0 does not contain `0xAA`, the default colour is used. This handles first-boot and corrupted data gracefully.

### HTTP API

| Method | Path | Description |
|---|---|---|
| GET | `/` | Serves the embedded HTML colour picker page |
| GET | `/color` | Returns current colour as JSON: `{"r":255,"g":150,"b":0}` |
| POST | `/color` | Sets colour. Body: `r=255&g=150&b=0` (form-urlencoded) |

### Web Interface

- Fully self-contained HTML/CSS/JavaScript embedded in firmware (stored in PROGMEM)
- No external CDN or script dependencies - works offline on the device's AP
- Mobile-responsive dark-themed UI
- HTML5 `<input type="color">` for colour selection
- Live preview circle with glow effect
- On save: LED updates immediately, colour is written to EEPROM

### File Structure

```
GudmunssonsLED/
├── platformio.ini        # Build configuration and library dependencies
├── src/
│   └── main.cpp          # All firmware source (single-file design)
├── include/              # (unused, for future headers)
├── lib/                  # (unused, for local libraries)
└── test/                 # (unused, for unit tests)
```

---

## WiFi Configuration

| Parameter | Value |
|---|---|
| Mode | Access Point (AP) |
| SSID | `GudmunssonLED` |
| Password | None (open) |
| IP address | `192.168.4.1` |
| Web UI URL | `http://192.168.4.1` |
