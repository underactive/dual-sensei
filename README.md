# Dual-Sensei

PS5 DualSense → PS1 wireless controller bridge using ESP32.

Play your original PlayStation 1 (1995) wirelessly with a PS5 DualSense controller. Two independent bridge units enable 2-player wireless play.

## How It Works

```
[DualSense] ──BT──▶ [ESP32 + OLED] ──SPI──▶ [PS1 Controller Port]
```

The ESP32 receives DualSense inputs via Bluetooth Classic HID, translates them to PS1's SPI-based controller protocol (digital pad, ID `0x41`), and responds as a slave device on the PS1 controller port. A 1.3" OLED + rotary encoder provide a local UI for configuration.

## Current Status

**v0.1.0 — Phase 1 (local UI validation)**

- [x] PlatformIO project with ESP32 Arduino framework
- [x] SH1106 128×64 OLED display (splash, pairing, visualizer, menu screens)
- [x] ISR-driven rotary encoder (quadrature state machine) + 2 navigation buttons
- [x] Menu system with NVS-persistent settings
- [ ] Bluetooth DualSense pairing (Phase 1, Epoch 2)
- [ ] PS5→PS1 button mapping + SPI slave protocol (Phase 1, Epoch 3)
- [ ] Real PS1 hardware integration (Phase 2)

## Hardware

| Component | Qty | Purpose |
|-----------|-----|---------|
| ESP32-WROOM-32 DevKit | 2 | MCU + BT radio (one per player) |
| SH1106 1.3" OLED + EC11 encoder + 2 buttons | 2 | UI board |
| Breadboard, jumpers, LEDs, resistors | — | Prototyping |
| PS1/PS2 extension cable (Phase 2) | 2 | PS1 port connection |

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
pio run                # Build
pio run -t upload      # Flash to ESP32
pio device monitor     # Serial monitor (115200 baud)
```

## Project Structure

```
src/
├── config.h          # Pin definitions, constants
├── main.cpp          # setup()/loop() entry point
├── display.h/.cpp    # U8g2 SH1106 OLED driver
├── input.h/.cpp      # Encoder + button ISRs, FreeRTOS queue
└── menu.h/.cpp       # Menu state machine, NVS settings
docs/
├── schematic.svg     # Visual wiring schematic
├── parts-list.md     # Component list with costs
└── wiring.md         # Pin-by-pin wiring guide
```

## License

MIT
