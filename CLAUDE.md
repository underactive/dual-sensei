# CLAUDE.md - Dual-Sensei Project Context

## Project Overview

**Dual-Sensei** is a PS5 DualSense-to-PS1 wireless controller bridge using an ESP32-WROOM-32, enabling wireless play on the original PlayStation 1 (1995). Two independent bridge units provide 2-player support.

**Current Version:** 0.1.0
**Status:** In development (Phase 1 — local UI validation, no PS1 connection yet)

---

## Hardware

### Microcontroller
- **ESP32-WROOM-32** (DevKit 30/38-pin)
- Dual-core Xtensa LX6 @ 240MHz
- Bluetooth Classic (BR/EDR) — for DualSense HID
- 520KB SRAM, 4MB Flash
- Powered via USB (PS1 port cannot supply enough current)

### Components
| Ref | Component | Purpose |
|-----|-----------|---------|
| U1 | ESP32-WROOM-32 DevKit | Main MCU, BT radio, SPI slave |
| D1 | SH1106 1.3" 128x64 OLED | Status display |
| E1 | EC11 rotary encoder | Menu navigation |
| SW1 | Confirm button (CON) | Menu confirm |
| SW2 | Back button (BAK) | Menu back |
| LED1-3 | Debug LEDs (Phase 1) | Visual output verification |

### Pin Assignments
| Pin | Function | Notes |
|-----|----------|-------|
| **I2C (OLED)** | | |
| 21 | I2C SDA | Display data |
| 22 | I2C SCL | Display clock |
| **Encoder** | | |
| 32 | Encoder A (TRA) | Quadrature channel A, internal pull-up |
| 33 | Encoder B (TRB) | Quadrature channel B, internal pull-up |
| 25 | Encoder Push (PHS) | Push switch, internal pull-up |
| **Buttons** | | |
| 26 | Confirm (CON) | Active-low, internal pull-up |
| 27 | Back (BAK) | Active-low, internal pull-up |
| **Debug LEDs** | | |
| 2 | BT status (onboard) | Also used as init indicator |
| 16 | Test LED 1 | 220Ω series resistor |
| 17 | Test LED 2 | 220Ω series resistor |
| **PS1 SPI — VSPI (Phase 2)** | | |
| 18 | SPI CLK (PS1 pin 7) | Input from PS1 |
| 23 | SPI MOSI / CMD (PS1 pin 2) | Input from PS1 |
| 19 | SPI MISO / DAT (PS1 pin 1) | Output, open-drain |
| 5 | SPI CS / ATT (PS1 pin 6) | Input from PS1 |
| 4 | ACK (PS1 pin 9) | Output, open-drain, manual GPIO pulse |

---

## Architecture

### Core Files
Modular C++ with Arduino setup()/loop() running under ESP-IDF. Each subsystem is a pair of .h/.cpp files with a C-style API (no classes).

- `src/main.cpp` — Entry point: Arduino setup()/loop(), orchestrates subsystems
- `src/config.h` — All GPIO pin definitions, constants, thresholds
- `src/display.h/.cpp` — U8g2 SH1106 OLED driver, screen rendering
- `src/input.h/.cpp` — ISR-driven encoder + button input, FreeRTOS event queue
- `src/menu.h/.cpp` — Menu state machine + NVS settings persistence

### Dependencies
- **PlatformIO** — Build system
- **ESP-IDF + Arduino** — Dual framework (`framework = espidf, arduino`)
- **U8g2** (`olikraus/U8g2`) — SH1106 OLED driver (via PlatformIO lib_deps)
- **Preferences** (Arduino built-in) — NVS key-value storage
- **Bluepad32** (Epoch 2) — DualSense Bluetooth HID

### Key Subsystems

#### 1. Input System (`input.h/.cpp`)
- Quadrature encoder uses a 16-entry state-machine lookup table — inherently rejects noise without delay-based debounce
- Buttons debounced via `esp_timer_get_time()` with 50ms threshold in ISR
- All input events pushed to a FreeRTOS queue, consumed non-blocking by `input_poll()` in the main loop
- `ENC_STEPS_PER_DETENT` (default 4) controls encoder sensitivity — adjust if encoder feels too fast/slow
- ISRs use `IRAM_ATTR` and `DRAM_ATTR` for reliability

#### 2. Display System (`display.h/.cpp`)
- U8g2 full-buffer mode (`_F_`) — 1KB RAM for 128x64 framebuffer
- Screen types: SPLASH, PAIRING, VISUALIZER, MENU
- `display_update()` throttled to ~15 FPS via `millis()` check
- Splash is one-shot (rendered once, not redrawn)
- Menu rendering queries `menu.h` getters — no circular header dependency

#### 3. Menu System (`menu.h/.cpp`)
- State machine: HOME → SETTINGS → SETTING_EDIT / PAIRING / ABOUT
- Encoder navigates items; CON enters/confirms; BAK goes back/discards
- Settings saved to NVS on CON, discarded on BAK (snapshot/restore pattern)
- Three persistent settings: trigger threshold (0-255), stick-to-dpad (bool), player number (1-2)

#### 4. Settings / Configuration Storage
```
NVS namespace: "dual-sensei"
Keys:
  "trig_thresh" — uint8_t (default 128)
  "stick_dpad"  — bool (default false)
  "player_num"  — uint8_t (default 1)
```
- Saved to ESP32 NVS via Arduino Preferences library
- Loaded on boot in `menu_init()`

---

## Build Configuration

### PlatformIO Configuration
- **`framework = arduino`** — Arduino-ESP32 (built on ESP-IDF internally). All ESP-IDF APIs accessible via direct includes (`driver/gpio.h`, `driver/spi_slave.h`, etc.). Avoids the managed component bloat of `framework = espidf, arduino`.
- **`board_build.partitions = partitions.csv`** — Custom partition table with ~1.9MB app partition (BT Classic stack is large)
- **`CORE_DEBUG_LEVEL=3`** — ESP32 debug logging at INFO level

### Environment Variables

| Variable | Purpose | Values |
|----------|---------|--------|
| `CORE_DEBUG_LEVEL` | ESP32 log verbosity | 0=None, 1=Error, 2=Warn, 3=Info, 4=Debug, 5=Verbose |

---

## Code Style

- **Language:** C++ (Arduino-style with ESP-IDF extensions)
- **Module pattern:** C-style API (free functions, not classes) — one .h/.cpp pair per subsystem
- **Indentation:** 4 spaces
- **Line length:** ~100 chars soft limit
- **Formatting:** `snprintf` with `sizeof(buf)` everywhere (bounded formatting)
- **ISR functions:** Must use `IRAM_ATTR`; data they access must use `DRAM_ATTR` or be `volatile`
- **Naming:** `snake_case` for functions and variables; `UPPER_CASE` for defines; `PascalCase` for enums

---

## Known Issues / Limitations

1. **EC11 encoder detent count** — `ENC_STEPS_PER_DETENT` is set to 4 (most common). If the display board's encoder has a different ratio, navigation will feel too fast or too slow. Adjust in `config.h`.
2. **GPIO ISR service double-install** — Arduino-ESP32 may pre-install the GPIO ISR service. `input_init()` tolerates `ESP_ERR_INVALID_STATE` from `gpio_install_isr_service()`.
3. **I2C display blocking** — `u8g2.sendBuffer()` blocks the main loop for ~20ms per frame. Acceptable in Phase 1, but Phase 3's SPI slave has hard real-time constraints. Will need display rendering in a separate FreeRTOS task.
4. **No unit tests yet** — Testable pure-logic functions exist (encoder table, menu state machine, value formatting) but test infrastructure is deferred to Epoch 3.

---

## Development Rules

### 1. Validate all external input at the boundary
Every value arriving from an external source (BT HID, SPI, user input) must be validated and clamped to valid bounds before being stored or used.

### 2. Guard all array-indexed lookups
Any value used as an index into an array must have a bounds check before access: `(val < COUNT) ? ARRAY[val] : fallback`.

### 3. Reset connection-scoped state on disconnect
BT connection state, input buffers, and SPI response buffers must be reset when a DualSense disconnects.

### 4. Avoid memory-fragmenting patterns in long-running code
Use stack-allocated buffers and fixed-size arrays in hot paths. Reserve `new`/`malloc` for one-shot init operations.

### 5. Use symbolic constants, not magic numbers
Pin numbers, protocol bytes, thresholds — all in `config.h`.

### 6. Throttle event-driven output
Display rendering capped at 15 FPS. Serial debug output rate-limited where applicable.

### 7. Use bounded string formatting
Always `snprintf(buf, sizeof(buf), ...)`. Never `sprintf`.

### 8. Report errors, don't silently fail
Log errors via `Serial.printf` with `[module]` prefix. Don't silently drop failed operations.

### 9. ISR safety
ISR functions: IRAM_ATTR, no heap allocation, no Serial calls, no blocking. Use `xQueueSendFromISR` for event passing. Data shared with ISR must be `volatile` or protected by `DRAM_ATTR`.

---

## Common Modifications

### Version bumps
Version string appears in 2 files:
1. `src/config.h` — `FW_VERSION` define
2. `CLAUDE.md` — Project Overview section

**Keep all version references in sync.**

### Add a new NVS setting
1. Add default define in `src/config.h`
2. Add variable + getter in `src/menu.h` / `src/menu.cpp`
3. Add NVS load in `menu_init()`
4. Add menu item in `MENU_LABELS[]`, increment `EDITABLE_COUNT` if editable
5. Add edit handling in `handle_edit()`
6. Add format case in `format_setting_value()`
7. Add save/discard/snapshot cases

### Add a new display screen
1. Add enum value in `src/display.h` `Screen` enum
2. Add render function `render_xxx()` in `src/display.cpp`
3. Add case to `display_update()` switch
4. If data-driven, add setter functions in `display.h`

---

## File Inventory

| File / Directory | Purpose |
|------------------|---------|
| `src/main.cpp` | Entry point: setup()/loop() orchestration |
| `src/config.h` | Pin definitions, constants, thresholds |
| `src/display.h/.cpp` | U8g2 SH1106 OLED driver + screen rendering |
| `src/input.h/.cpp` | ISR encoder + button input, FreeRTOS queue |
| `src/menu.h/.cpp` | Menu state machine + NVS settings |
| `platformio.ini` | PlatformIO build configuration |
| `sdkconfig.defaults` | ESP-IDF SDK defaults (BT, SPI, CPU) |
| `partitions.csv` | Custom flash partition table |
| `.gitignore` | Git ignores for PlatformIO/ESP-IDF/IDE |
| `CLAUDE.md` | This file |
| `CLAUDE_TEMPLATE.md` | Template this file was based on |
| `docs/CLAUDE.md/plans/` | Plan, implementation, and audit records |
| `docs/CLAUDE.md/version-history.md` | Changelog |
| `docs/CLAUDE.md/testing-checklist.md` | Manual QA checklist |
| `docs/CLAUDE.md/future-improvements.md` | Ideas backlog |

---

## Build Instructions

### Prerequisites
- PlatformIO Core CLI (`pio`) or PlatformIO IDE
- USB cable matching ESP32 DevKit connector

### Quick Start
```bash
pio run              # Build firmware
pio run -t upload    # Flash to ESP32
pio device monitor   # Serial monitor (115200 baud)
```

### Troubleshooting Build
- **"Arduino.h not found"** — Ensure `framework = espidf, arduino` is set (both frameworks required)
- **Partition table errors** — Verify `partitions.csv` offsets don't overlap and total ≤ 4MB
- **BT stack link errors** — Custom partition with ~1.9MB app space is required; default partition is too small

---

## Testing

See `docs/CLAUDE.md/testing-checklist.md` for the full QA testing checklist.

---

## Future Improvements

See `docs/CLAUDE.md/future-improvements.md` for the ideas backlog.

---

## Maintaining This File

### Keep CLAUDE.md in sync with the codebase
**Every plan that adds, removes, or changes a feature must include CLAUDE.md updates as part of the implementation.** Treat CLAUDE.md as a living spec — if the code and this file disagree, this file is wrong and must be fixed before the work is considered complete.

### When to update CLAUDE.md
- **Adding a new subsystem or module** — add it to Architecture and File Inventory
- **Adding a new setting or config field** — update the Settings section and Common Modifications
- **Discovering a new bug class** — add a Development Rule to prevent recurrence
- **Changing the build process** — update Build Instructions and/or Build Configuration
- **Bumping the version** — update the version in Project Overview
- **Adding/removing files** — update File Inventory
- **Finding a new limitation** — add to Known Issues

### Supplementary docs
For sections that grow large, move them to separate files under `docs/` and link from here.

### Version history maintenance
See `docs/CLAUDE.md/version-history.md`.

---

## Origin

Created with Claude (Anthropic)
