# Plan: PS2 Scope Expansion

## Objective

Expand Dual-Sensei from PS1-only to PS1+PS2 support. PS2 uses the same SPI bus/connector but adds analog sticks, L3/R3 buttons, and different device IDs. This change expands the data model and visualizer now (Epoch 2), while the SPI protocol differences are Phase 3 work.

## Changes

### 1. `main/config.h` — PS2 protocol constants + pin renames
- Rename `PIN_PS1_*` → `PIN_PSX_*` (shared between PS1/PS2)
- Add `PS2_DEVICE_ID_ANALOG`, `PS2_STICK_CENTER`, `CONSOLE_MODE_DEFAULT`
- Rename `PS1_DATA_MARKER` → `PSX_DATA_MARKER`, `PS1_IDLE_BYTE` → `PSX_IDLE_BYTE`

### 2. `main/display.h` — Expand ControllerState
- Add `bool l3, r3` (stick press buttons)
- Add `uint8_t lx, ly, rx, ry` (analog stick axes, 0-255, 128=center)

### 3. `main/bt.cpp` — Map DualSense sticks and L3/R3
- Add `axis_to_ps2()` helper for Bluepad32→PS2 axis conversion
- Map `thumbL()`/`thumbR()` → L3/R3
- Map stick axes from signed (-511..512) to unsigned (0-255)

### 4. `main/menu.h` / `main/menu.cpp` — Add "Console Mode" setting
- New NVS setting `con_mode` (uint8_t, 0=PS1, 1=PS2), setting_id=4
- Full 8-point recipe: default, variable+getter, NVS load, MenuItem, edit handler, format, save/discard/snapshot, min/max
- Update action item indices (+1 each)

### 5. `main/display.cpp` — Visualizer with analog sticks
- Add `draw_analog_stick()` helper (circle + position dot, filled when pressed)
- Replace `compute_ps1_bytes()` → `compute_protocol_bytes()` (PS1: 2 bytes, PS2: 6 bytes with L3/R3 + sticks)
- Redesign visualizer layout: compressed shoulders/buttons + stick circles below
- Update about screen text to "PSX"

### 6. `CLAUDE.md` — Documentation updates
- Project overview: PS1+PS2
- Pin table: PSX naming
- ControllerState: L3/R3 + sticks
- Settings section: console_mode NVS key
- Visualizer description: sticks + console mode

## Dependencies

- config.h and display.h must be updated before bt.cpp and menu.cpp (they include these headers)
- menu.h getter must exist before display.cpp can call `menu_get_console_mode()`

## Risks / Open Questions

- Face button radius reduced from 5 to 4 pixels to fit sticks — symbols may be harder to see on small OLED
- PS2 protocol bytes line uses condensed format to fit 128px width
- Stick position dot is 2x2 pixels in a r=5 circle — may need refinement after testing on hardware
