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


see also the DESIGN.md file
