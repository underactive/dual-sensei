# Dual-Sensei

Wireless controller → PS1/PS2 bridge using ESP32.

Play your original PlayStation 1 (1995) and PlayStation 2 (2000) wirelessly with a PS5 DualSense, PS4 DualShock 4, Xbox One (Bluetooth), or Nintendo Switch Pro controller. Two independent bridge units enable 2-player wireless play.

## How It Works

```
[Controller] ──BT──▶ [ESP32 + OLED] ──SPI──▶ [PS1/PS2 Controller Port]
```

The ESP32 receives controller inputs via Bluetooth (using Bluepad32), translates them to the PS1/PS2 SPI-based controller protocol, and responds as a slave device on the controller port. PS1 mode emulates a digital pad (ID `0x41`); PS2 mode emulates a DualShock 2 with analog sticks (ID `0x73`). A 1.3" OLED + rotary encoder provide a local UI for configuration.

**Supported controllers:** DualSense (PS5), DualShock 4 (PS4), Xbox One (Bluetooth), Switch Pro

## Current Status

**v0.2.0 — Epoch 2 (controller BT connected, no PS1/PS2 SPI connection yet)**

- [x] PlatformIO project with ESP-IDF + Arduino component framework
- [x] SH1106 128×64 OLED display (splash, pairing, visualizer, menu screens)
- [x] ISR-driven rotary encoder (quadrature state machine) + 2 navigation buttons
- [x] Menu system with NVS-persistent settings (ghost_operator-style UI)
- [x] Bluepad32 Bluetooth pairing + button/trigger/stick mapping
- [x] Multi-controller support: DualSense, DualShock 4, Xbox One, Switch Pro
- [x] PS2 scope expansion: console mode setting, analog sticks, L3/R3, mode-dependent visualizer
- [x] Touchpad-to-Select/Start mapping (DualSense + DualShock 4)
- [x] Rumble API + test rumble menu action
- [ ] PS1/PS2 SPI slave protocol (Phase 2)
- [ ] Real PS1/PS2 hardware integration (Phase 2)

## Hardware

| Component | Qty | Purpose |
|-----------|-----|---------|
| ESP32-WROOM-32 DevKit | 2 | MCU + BT radio (one per player) |
| SH1106 1.3" OLED + EC11 encoder + 2 buttons | 2 | UI board |
| Breadboard, jumpers, LEDs, resistors | — | Prototyping |
| PS1/PS2 extension cable (Phase 2) | 2 | Controller port connection |

Full parts list: [`docs/parts-list.md`](docs/parts-list.md)

## Wiring

See [`docs/wiring.md`](docs/wiring.md) for pin-by-pin connections and [`docs/schematic.svg`](docs/schematic.svg) for the visual schematic.

**Display board header (9-pin):**
```
Header:  CON  SDA  SCL  PHS  TRA  TRB  BAK  GND  VCC
ESP32:   G26  G21  G22  G25  G32  G33  G27  GND  3V3
```

## Build & Flash

Requires [PlatformIO](https://platformio.org/).

```bash
make build       # Build firmware (pio run)
make flash       # Build + flash to ESP32 (pio run -t upload)
make monitor     # Serial monitor at 115200 baud (pio device monitor)
make clean       # Clean build artifacts (pio run -t clean)
```

Or directly with PlatformIO:
```bash
pio run                # Build
pio run -t upload      # Flash to ESP32
pio device monitor     # Serial monitor (115200 baud)
```

## Project Structure

```
main/
├── main.c            # BTstack/Bluepad32 bootstrap (CPU0)
├── main.cpp          # Arduino setup()/loop() entry point (CPU1)
├── config.h          # Pin definitions, constants
├── bt.h/.cpp         # DualSense BT host (Bluepad32)
├── display.h/.cpp    # U8g2 SH1106 OLED driver
├── input.h/.cpp      # Encoder + button ISRs, FreeRTOS queue
└── menu.h/.cpp       # Menu state machine, NVS settings
components/
├── arduino/          # Arduino-ESP32 as ESP-IDF component
├── bluepad32/        # Bluepad32 BT host library
├── bluepad32_arduino/# Bluepad32 Arduino bindings
└── btstack/          # BTstack Bluetooth host stack
docs/
├── schematic.svg     # Visual wiring schematic
├── parts-list.md     # Component list with costs
└── wiring.md         # Pin-by-pin wiring guide
```

## License

MIT
