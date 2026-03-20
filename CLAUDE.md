# CLAUDE.md - Dual-Sensei Project Context

## Project Overview

**Dual-Sensei** is a wireless controller-to-PS1/PS2 bridge using an ESP32-WROOM-32, enabling wireless play on the original PlayStation 1 (1995) and PlayStation 2 (2000). Supports PS5 DualSense, PS4 DualShock 4, Xbox One (Bluetooth), and Nintendo Switch Pro controllers. Two independent bridge units provide 2-player support.

**Current Version:** 0.2.0
**Status:** In development (Epoch 2 — DualSense BT connected, no PS1/PS2 connection yet)

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
| **PSX SPI — VSPI (Phase 2)** | | Shared between PS1 and PS2 (same connector) |
| 18 | SPI CLK (PSX pin 7) | Input from console |
| 23 | SPI MOSI / CMD (PSX pin 2) | Input from console |
| 19 | SPI MISO / DAT (PSX pin 1) | Output, open-drain |
| 5 | SPI CS / ATT (PSX pin 6) | Input from console |
| 4 | ACK (PSX pin 9) | Output, open-drain, manual GPIO pulse |

---

## Architecture

### Core Files
Modular C++ with Arduino setup()/loop() running under ESP-IDF. Each subsystem is a pair of .h/.cpp files with a C-style API (no classes). Source lives in `main/` (ESP-IDF convention).

- `main/main.c` — BTstack/Bluepad32 bootstrap (runs on CPU0, launches Arduino on CPU1)
- `main/main.cpp` — Arduino entry point: setup()/loop(), orchestrates subsystems
- `main/config.h` — All GPIO pin definitions, constants, thresholds
- `main/bt.h/.cpp` — Bluepad32 DualSense BT host, controller data mapping
- `main/display.h/.cpp` — U8g2 SH1106 OLED driver, screen rendering, ControllerState struct
- `main/input.h/.cpp` — ISR-driven encoder + button input, FreeRTOS event queue
- `main/menu.h/.cpp` — Menu state machine + NVS settings persistence

### Dependencies
- **PlatformIO** — Build system (pioarduino platform fork for ESP-IDF + Arduino component support)
- **ESP-IDF** v5.4.2 — Framework (`framework = espidf` in platformio.ini)
- **Arduino-ESP32** v3.2.1 — Included as ESP-IDF component in `components/arduino`
- **Bluepad32** v4.2.0 — DualSense Bluetooth HID host (`components/bluepad32` + `components/bluepad32_arduino`)
- **BTstack** — Bluetooth host stack used by Bluepad32 (`components/btstack`, replaces ESP-IDF Bluedroid)
- **U8g2** (`olikraus/U8g2`) — SH1106 OLED driver (via PlatformIO lib_deps)
- **Preferences** (Arduino built-in) — NVS key-value storage

### Key Subsystems

#### 1. Input System (`input.h/.cpp`)
- Quadrature encoder uses a 16-entry state-machine lookup table — inherently rejects noise without delay-based debounce
- Buttons use armed/disarmed debounce pattern: ISR fires on `GPIO_INTR_ANYEDGE`, checks 50ms time-gate + pin-level LOW + armed flag. On press: queues event and disarms. Re-arming happens ONLY in `input_poll()` (main loop) after pin reads stable HIGH for the debounce period. This guarantees exactly one event per physical press regardless of bounce duration or hold time.
- All input events pushed to a FreeRTOS queue, consumed non-blocking by `input_poll()` in the main loop
- `input_flush_queue()` resets the queue — called on menu entry to prevent stale encoder events from carrying over
- `ENC_STEPS_PER_DETENT` (default 4) controls encoder sensitivity — adjust if encoder feels too fast/slow
- ISRs use `IRAM_ATTR` and `DRAM_ATTR` for reliability
- GPIO ISR service serializes both encoder channel handlers on single core

#### 2. Display System (`display.h/.cpp`)
- U8g2 full-buffer mode (`_F_`) — 1KB RAM for 128x64 framebuffer
- Screen types: SPLASH, PAIRING, VISUALIZER, MENU
- `display_update()` throttled to ~15 FPS via `millis()` check
- Splash is one-shot (rendered once, not redrawn)
- Menu rendering queries `menu.h` getters — no circular header dependency
- **Visualizer screen**: Mode-dependent controller layout driven by `ControllerState` struct and console mode setting. Status line shows controller type name (e.g. "DualSense", "XBox One") via `bt_get_controller_name()`, or "No Controller" when disconnected. **PS1 mode**: original digital pad layout — D-pad, face buttons (△○×□, r=5), shoulders (L1/L2/R1/R2), Select/Start, "PS1: XX XX" protocol bytes. **PS2 mode**: DualShock 2 layout — same buttons plus L3/R3 text labels in shoulder row (highlighted on press), analog stick circles (r=4) positioned between D-pad/face and Select/Start matching the physical controller layout, "PS2:XXYY XXYY XXYY" protocol bytes (6 bytes).
- **ControllerState**: Struct in `display.h` with 16 button bools (including L3/R3) + 4 analog stick axes (lx/ly/rx/ry, 0-255, 128=center) + `connected` flag, passed via `display_set_controller()`

#### 3. Bluetooth System (`bt.h/.cpp`)
- **Bluepad32** BT host for wireless controllers (BR/EDR + BLE via BTstack on CPU0)
- **Supported controllers**: DualSense (PS5), DualShock 4 (PS4), Xbox One (BT), Switch Pro — all use Bluepad32's unified gamepad API. Button mapping, triggers, sticks, and rumble work identically across all controller types with no controller-specific code paths.
- Callbacks (`on_connected`, `on_disconnected`) run on CPU1 inside `BP32.update()` — same task as Arduino `loop()`
- `bt_update()` polls Bluepad32 and maps controller data to `ControllerState`
- `bt_get_controller_name()` returns the display-friendly name of the connected controller (e.g. `"DualSense"`, `"DualShock 4"`, `"XBox One"`, `"Switch Pro"`)
- **Button mapping**: Bluepad32 uses positional Xbox-style naming (A=south=Cross, B=east=Circle, X=west=Square, Y=north=Triangle) — correct for all controller types
- **Analog triggers**: L2/R2 provide 0-1023 analog values, thresholded against `trigger_threshold` NVS setting (0-255, scaled via `/4`)
- **Analog sticks**: Bluepad32 gives -511..512 signed; converted to PS2 range 0-255 (128=center) via `axis_to_ps2()`. Stored in ControllerState for visualizer and future SPI protocol.
- **Stick-to-DPad**: When enabled, left stick axes (±512 range) are OR'd into D-pad bools using 128 deadzone
- **Touchpad**: DS4 and DualSense touchpads exposed as virtual mouse device via `enableVirtualDevice(true)`. Maps left half → Select, right half → Start when "Touchpad Sel/St" setting is ON. Inactive for Xbox/Switch (no touchpad, `connected_mouse` stays `nullptr`).
- **Pairing**: `bt_start_pairing()` enables BT scanning, `bt_stop_pairing()` disables it. Pairing gestures: DS4/DualSense — Share/Create+PS hold; Xbox — pair button 3s; Switch Pro — sync button.
- Single-controller design: rejects additional connections after first controller pairs

#### 4. Menu System (`menu.h/.cpp`)
- State machine: HOME → SETTINGS → SETTING_EDIT / PAIRING / ABOUT
- **Data-driven menu**: `MENU_ITEMS[]` array of `MenuItem` structs with `type` (HEADING/VALUE/ACTION), `label`, `help` text, and `setting_id`
- Three item types: **MENU_HEADING** (non-selectable section divider), **MENU_VALUE** (editable setting), **MENU_ACTION** (navigable action)
- `find_next_selectable()` skips heading rows during encoder navigation
- Ghost_operator-style rendering: full-row inversion for selected items, partial-row inversion with `< >` arrows for inline editing, context-aware help bar at bottom
- Encoder push (PHS) treated as CON in all menu states for consistent behavior
- Queue flushed on menu entry to prevent stale encoder events
- Settings saved to NVS on CON, discarded on BAK (snapshot/restore pattern)
- Four persistent settings: trigger threshold (0-255), stick-to-dpad (bool), player number (1-2), console mode (0=PS1, 1=PS2)
- Nine menu items across two groups: Controller (Trigger Thresh, Stick to DPad, Player Number, Touchpad Sel/St, Console Mode) and Device (Test Rumble, Pairing, About)

#### 4. Settings / Configuration Storage
```
NVS namespace: "dual-sensei"
Keys:
  "trig_thresh" — uint8_t (default 128)
  "stick_dpad"  — bool (default false)
  "player_num"  — uint8_t (default 1)
  "con_mode"    — uint8_t (default 0, 0=PS1, 1=PS2)
```
- Saved to ESP32 NVS via Arduino Preferences library
- Loaded on boot in `menu_init()` with range validation (player_num clamped to 1-2)

---

## Build Configuration

### PlatformIO Configuration
- **`framework = espidf`** — ESP-IDF v5.4.2 with Arduino-ESP32 as a component (in `components/arduino`). All Arduino and ESP-IDF APIs are accessible. Uses pioarduino platform fork for ESP-IDF + Arduino component support.
- **`src_dir = main`** — Source files in `main/` per ESP-IDF convention (set in `[platformio]` section)
- **`lib_compat_mode = off`** — Required for U8g2 (Arduino library) to work with ESP-IDF framework
- **`board_build.partitions = partitions.csv`** — Custom partition table with ~1.9MB app partition (BTstack + Bluepad32 + Arduino component)
- **`board_build.embed_txtfiles`** — Certificate files required by Arduino component's transitive deps (ESP Insights/Rainmaker). Not used by Dual-Sensei but must exist for linking.
- **`upload_speed = 460800`** — Fastest reliable speed for most USB-serial chips; lower to 115200 if upload fails

### Environment Variables

| Variable | Purpose | Values |
|----------|---------|--------|
| `CORE_DEBUG_LEVEL` | ESP32 log verbosity | 0=None, 1=Error, 2=Warn, 3=Info, 4=Debug, 5=Verbose |

Environment files / define sources:
- `platformio.ini` — `build_flags = -DCORE_DEBUG_LEVEL=3` (INFO level)
- `sdkconfig.defaults` — Reference config for ESP-IDF settings (not auto-applied with Arduino framework; documents intended BT/SPI/CPU config)

---

## Code Style

- **Language:** C++ (Arduino-style with ESP-IDF extensions)
- **Module pattern:** C-style API (free functions, not classes) — one .h/.cpp pair per subsystem
- **Indentation:** 4 spaces
- **Line length:** ~100 chars soft limit
- **Formatting:** `snprintf` with `sizeof(buf)` everywhere (bounded formatting)
- **ISR functions:** Must use `IRAM_ATTR`; data they access must use `DRAM_ATTR` or be `volatile`
- **Naming:** `snake_case` for functions and variables; `UPPER_CASE` for defines; `PascalCase` for enums and structs

---

## External Integrations

### Bluepad32 v4.2.0
- **What:** Multi-controller Bluetooth HID host library for ESP32 using BTstack
- **Supported controllers:** DualSense (PS5), DualShock 4 (PS4), Xbox One (BT), Switch Pro — all mapped through a unified gamepad API (`Controller` class). The library's per-controller HID parsers normalize all inputs to a single `uni_gamepad_t` struct, so no controller-specific code is needed in the application layer.
- **Loaded via:** ESP-IDF component in `components/bluepad32` + `components/bluepad32_arduino` + `components/btstack`
- **Source:** Official [esp-idf-arduino-bluepad32-template](https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template) — components copied from template
- **Architecture:** BTstack runs on CPU0 (`main.c` bootstraps it via `btstack_init()` + `uni_init()`), Arduino runs on CPU1. `BP32.update()` in the Arduino loop polls for controller data with mutex-protected cross-CPU access.
- **sdkconfig requirements:** `CONFIG_BT_CONTROLLER_ONLY=y` (disables Bluedroid to avoid symbol collisions with BTstack), `CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE=n` (to keep Arduino Serial working), `CONFIG_AUTOSTART_ARDUINO=n` (main.c handles bootstrap)
- **Protocols:** BR/EDR (DualSense, DualShock 4, Switch Pro, Xbox fw v3.x/v4.x) and BLE (Xbox fw v5.x+). ESP32-WROOM supports both; ESP32-S3/C3/C6/H2 support BLE only.
- **Gotchas:** Bluepad32 uses Xbox-style positional button naming (A=south=Cross, B=east=Circle, X=west=Square, Y=north=Triangle) — correct for all controller types. Analog triggers are 0-1023 range (not 0-255). Console class conflicts with Serial if `CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE=y`. `getModelName()` returns Arduino `String` (heap-allocated) — copy to fixed buffer promptly to avoid fragmentation.

---

## Known Issues / Limitations

1. **EC11 encoder detent count** — `ENC_STEPS_PER_DETENT` is set to 4 (most common). If the display board's encoder has a different ratio, navigation will feel too fast or too slow. Adjust in `config.h`.
2. **GPIO ISR service double-install** — Arduino-ESP32 may pre-install the GPIO ISR service. `input_init()` tolerates `ESP_ERR_INVALID_STATE` from `gpio_install_isr_service()`.
3. **I2C display blocking** — `u8g2.sendBuffer()` blocks the main loop for ~20ms per frame. Acceptable in Phase 1, but Phase 3's SPI slave has hard real-time constraints. Will need display rendering in a separate FreeRTOS task.
4. **No unit tests yet** — Testable pure-logic functions exist (encoder table, menu state machine, value formatting) but test infrastructure is deferred to Epoch 3.
5. **sdkconfig.defaults vs sdkconfig.esp32** — PlatformIO generates `sdkconfig.esp32` from `sdkconfig.defaults`. If `sdkconfig.esp32` exists with stale values, delete it to regenerate from defaults. Both files are gitignored.

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

## Plan Pre-Implementation

Before planning, check `docs/CLAUDE.md/plans/` for prior plans that touched the same areas. Scan the **Files changed** lists in both `implementation.md` and `audit.md` files to find relevant plans without reading every file — then read the full `plan.md` only for matches. This keeps context window usage low while preserving access to project history.

When a plan is finalized and about to be implemented, write the full plan to `docs/CLAUDE.md/plans/{epoch}-{plan_name}/plan.md`, where `{epoch}` is the Unix timestamp at the time of writing and `{plan_name}` is a short kebab-case description of the plan (e.g., `1709142000-add-user-auth/plan.md`).

The epoch prefix ensures chronological ordering — newer plans visibly supersede earlier ones at a glance based on directory name ordering.

The plan document should include:
- **Objective** — what is being implemented and why
- **Changes** — files to modify/create, with descriptions of each change
- **Dependencies** — any prerequisites or ordering constraints between changes
- **Risks / open questions** — anything flagged during planning that needs attention

---

## Plan Post-Implementation

After a plan has been fully implemented, write the completed implementation record to `docs/CLAUDE.md/plans/{epoch}-{plan_name}/implementation.md`, using the same directory as the corresponding `plan.md`.

The implementation document **must** include:
- **Files changed** — list of all files created, modified, or deleted. This section is **required** — it serves as a lightweight index for future planning, allowing prior plans to be found by scanning file lists without reading full plan contents.
- **Summary** — what was actually implemented (noting any deviations from the plan)
- **Verification** — steps taken to verify the implementation is correct (tests run, manual checks, build confirmation)
- **Follow-ups** — any remaining work, known limitations, or future improvements identified during implementation

If the implementation added or changed user-facing behavior (new settings, UI modes, protocol commands, or display changes), add corresponding `- [ ]` test items to `docs/CLAUDE.md/testing-checklist.md`. Each item should describe the expected observable behavior, not the implementation detail.

---

## Post-Implementation Audit

After finishing implementation of a plan, run the following subagents **in parallel** to audit all changed files.

> **Scope directive for all subagents:** Only flag issues in the changed code and its immediate dependents. Do not audit the entire codebase.

> **Output directive:** After all subagents complete, write a single consolidated audit report to `docs/CLAUDE.md/plans/{epoch}-{plan_name}/audit.md`, using the same directory as the corresponding `plan.md`. The audit report **must** include a **Files changed** section listing all files where findings were flagged. This section is **required** — it serves as a lightweight index for future planning, covering files affected by audit findings (including immediate dependents not in the original implementation).

### 1. QA Audit (subagent)
Review changes for:
- **Functional correctness**: broken workflows, missing error/loading states, unreachable code paths, logic that doesn't match spec
- **Edge cases**: empty/null/undefined inputs, zero-length collections, off-by-one errors, race conditions, boundary values (min/max/overflow)
- **Infinite loops**: unbounded `while`/recursive calls, callbacks triggering themselves, retry logic without max attempts or backoff
- **Performance**: unnecessary computation in hot paths, O(n²) or worse in loops over growing data, unthrottled event handlers, expensive operations blocking main thread or interrupt context

### 2. Security Audit (subagent)
Review changes for:
- **Injection / input trust**: unsanitized external input used in commands, queries, or output rendering; format string vulnerabilities; untrusted data used in control flow
- **Overflows**: unbounded buffer writes, unguarded index access, integer overflow/underflow in arithmetic, unchecked size parameters
- **Memory leaks**: allocated resources not freed on all exit paths, event/interrupt handlers not deregistered on cleanup, growing caches or buffers without eviction or bounds
- **Hard crashes**: null/undefined dereferences without guards, unhandled exceptions in async or interrupt context, uncaught error propagation across module boundaries

### 3. Interface Contract Audit (subagent)
Review changes for:
- **Data shape mismatches**: caller assumptions that diverge from actual API/protocol schema, missing fields treated as present, incorrect type coercion or endianness
- **Error handling**: no distinction between recoverable and fatal errors, swallowed failures, missing retry/backoff on transient faults, no timeout or watchdog configuration
- **Auth / privilege flows**: credential or token lifecycle issues, missing permission checks, race conditions during handshake or session refresh
- **Data consistency**: optimistic state updates without rollback on failure, stale cache served after mutation, sequence counters or cursors not invalidated after writes

### 4. State Management Audit (subagent)
Review changes for:
- **Mutation discipline**: shared state modified outside designated update paths, state transitions that skip validation, side effects hidden inside getters or read operations
- **Reactivity / observation pitfalls**: mutable updates that bypass change detection or notification mechanisms, deeply nested state triggering unnecessary cascading updates
- **Data flow**: excessive pass-through of context across layers where a shared store or service belongs, sibling modules communicating via parent state mutation, event/signal spaghetti without cleanup
- **Sync issues**: local copies shadowing canonical state, multiple sources of truth for the same entity, concurrent writers without arbitration (locks, atomics, or message ordering)

### 5. Resource & Concurrency Audit (subagent)
Review changes for:
- **Concurrency**: data races on shared memory, missing locks/mutexes/atomics around critical sections, deadlock potential from lock ordering, priority inversion in RTOS or threaded contexts
- **Resource lifecycle**: file handles, sockets, DMA channels, or peripherals not released on error paths; double-free or use-after-free; resource exhaustion under sustained load
- **Timing**: assumptions about execution order without synchronization, spin-waits without yield or timeout, interrupt latency not accounted for in real-time constraints
- **Power & hardware**: peripherals left in active state after use, missing clock gating or sleep transitions, watchdog not fed on long operations, register access without volatile or memory barriers

### 6. Testing Coverage Audit (subagent)
Review changes for:
- **Missing tests**: new public functions/modules without corresponding unit tests, modified branching logic without updated assertions, deleted tests not replaced
- **Test quality**: assertions on implementation details instead of behavior, tests coupled to internal structure, mocked so heavily the test proves nothing
- **Integration gaps**: cross-module flows tested only with mocks and never with integration or contract tests, initialization/shutdown sequences untested, error injection paths uncovered
- **Flakiness risks**: tests dependent on timing or sleep, shared mutable state between test cases, non-deterministic data (random IDs, timestamps), hardware-dependent tests without abstraction layer

### 7. DX & Maintainability Audit (subagent)
Review changes for:
- **Readability**: functions exceeding ~50 lines, boolean parameters without named constants, magic numbers/strings without explanation, nested ternaries or conditionals deeper than one level
- **Dead code**: unused includes/imports, unreachable branches behind stale feature flags, commented-out blocks with no context, exported symbols with zero consumers
- **Naming & structure**: inconsistent naming conventions, business/domain logic buried in UI or driver layers, utility functions duplicated across modules
- **Documentation**: public API changes without updated doc comments, non-obvious workarounds missing a `// WHY:` comment, breaking changes without migration notes

---

## Audit Post-Implementation

After audit findings have been addressed, update the `implementation.md` file in the corresponding `docs/CLAUDE.md/plans/{epoch}-{plan_name}/` directory:

1. **Flag fixed items** — In the audit report (`docs/CLAUDE.md/plans/{epoch}-{plan_name}/audit.md`), mark each finding that was fixed with a `[FIXED]` prefix so it is visually distinct from unresolved items.

2. **Append a fixes summary** — Add an `## Audit Fixes` section at the end of `implementation.md` containing:
   - **Fixes applied** — a numbered list of each fix, referencing the audit finding it addresses (e.g., "Fixed unchecked index access flagged by Security Audit §2")
   - **Verification checklist** — a `- [ ]` checkbox list of specific tests or manual checks to confirm each fix is correct (e.g., "Verify bounds check on `configIndex` with out-of-range input returns fallback")

3. **Leave unresolved items as-is** — Any audit findings intentionally deferred or accepted as-is should remain unmarked in the audit report. Add a brief note in the fixes summary explaining why they were not addressed.

4. **Update testing checklist** — If any audit fixes changed user-facing behavior, add corresponding `- [ ]` test items to `docs/CLAUDE.md/testing-checklist.md`. Each item should describe the expected observable behavior, not the implementation detail.

---

## Common Modifications

### Version bumps
Version string appears in 2 files:
1. `main/config.h` — `FW_VERSION` define
2. `CLAUDE.md` — Project Overview section

**Keep all version references in sync.**

### Add a new NVS setting
1. Add default define in `main/config.h`
2. Add variable + getter in `main/menu.h` / `main/menu.cpp`
3. Add NVS load in `menu_init()` with range validation
4. Add `MenuItem` entry in `MENU_ITEMS[]` with unique `setting_id`, type `MENU_VALUE`, label, and help text
5. Add `setting_id` case in `handle_edit()` for value adjustment (ENC_CW/CCW)
6. Add `setting_id` case in `format_setting_value()` for display formatting
7. Add `setting_id` cases in `save_current_setting()`, `discard_current_setting()`, `snapshot_current_setting()`
8. Add `setting_id` cases in `menu_is_at_min()` and `menu_is_at_max()` for arrow visibility

### Add a new display screen
1. Add enum value in `main/display.h` `Screen` enum
2. Add render function `render_xxx()` in `main/display.cpp`
3. Add case to `display_update()` switch
4. If data-driven, add setter functions in `display.h`

### Add a new menu action item (non-editable)
1. Add `MenuItem` entry in `MENU_ITEMS[]` with type `MENU_ACTION`, label, and help text
2. Add dispatch case in `handle_settings()` CON/PHS handler (match by label first character or use a named constant)
3. Add new `MenuState` enum value in `main/menu.h` if it has its own screen
4. Add handler function (e.g., `handle_xxx()`) with BAK → return to settings
5. Add dispatch case in `menu_handle_input()`

---

## File Inventory

| File / Directory | Purpose |
|------------------|---------|
| `main/main.c` | BTstack/Bluepad32 bootstrap (CPU0 entry point) |
| `main/main.cpp` | Arduino entry point: setup()/loop() orchestration |
| `main/config.h` | Pin definitions, constants, thresholds |
| `main/bt.h/.cpp` | Bluepad32 DualSense BT host, controller data mapping |
| `main/display.h/.cpp` | U8g2 SH1106 OLED driver + screen rendering + ControllerState struct |
| `main/input.h/.cpp` | ISR encoder + button input, FreeRTOS queue |
| `main/menu.h/.cpp` | Menu state machine + NVS settings |
| `main/CMakeLists.txt` | ESP-IDF component registration for main sources |
| `CMakeLists.txt` | Top-level ESP-IDF project file |
| `platformio.ini` | PlatformIO build configuration (espidf framework, pioarduino platform) |
| `sdkconfig.defaults` | ESP-IDF SDK config (BT, Bluepad32, Arduino, FreeRTOS settings) |
| `partitions.csv` | Custom flash partition table (1.9MB app, 20KB NVS, 64KB SPIFFS) |
| `components/` | ESP-IDF components: arduino, bluepad32, bluepad32_arduino, btstack (gitignored, from template) |
| `Makefile` | Build shortcuts (`make build`, `make flash`, `make monitor`, `make clean`) |
| `pio_patches.py` | PlatformIO extra_scripts: strips `--ng` from esp_idf_size (pioarduino compat) |
| `.gitignore` | Git ignores for PlatformIO/ESP-IDF/IDE/macOS/compiled artifacts |
| `CLAUDE.md` | This file |
| `CLAUDE_TEMPLATE.md` | Template this file was based on |
| `docs/CLAUDE.md/plans/` | Plan, implementation, and audit records (epoch-prefixed directories) |
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
make build       # Build firmware (pio run)
make flash       # Build + flash to ESP32 (pio run -t upload)
make monitor     # Serial monitor at 115200 baud (pio device monitor)
make clean       # Clean build artifacts (pio run -t clean)
```

Or directly with PlatformIO:
```bash
pio run              # Build firmware
pio run -t upload    # Flash to ESP32
pio device monitor   # Serial monitor (115200 baud)
```

### Troubleshooting Build
- **"Arduino.h not found"** — Ensure `framework = arduino` is set in `platformio.ini`
- **Partition table errors** — Verify `partitions.csv` offsets don't overlap and total ≤ 4MB
- **BT stack link errors** — Custom partition with ~1.9MB app space is required; default partition is too small
- **Upload failures** — Lower `upload_speed` to 115200 in `platformio.ini` if 460800 is unreliable

---

## Testing

See `docs/CLAUDE.md/testing-checklist.md` for the full QA testing checklist.

---

## Future Improvements

See `docs/CLAUDE.md/future-improvements.md` for the ideas backlog.

---

## Maintaining This File

### Keep CLAUDE.md in sync with the codebase
**Every plan that adds, removes, or changes a feature must include CLAUDE.md updates as part of the implementation.** Treat CLAUDE.md as a living spec — if the code and this file disagree, this file is wrong and must be fixed before the work is considered complete. During plan post-implementation, verify that all sections affected by the change are accurate. If a feature is removed, delete its documentation here rather than leaving stale references.

### When to update CLAUDE.md
- **Adding a new subsystem or module** — add it to Architecture and File Inventory
- **Adding a new setting or config field** — update the Settings section and Common Modifications
- **Discovering a new bug class** — add a Development Rule to prevent recurrence
- **Changing the build process** — update Build Instructions and/or Build Configuration
- **Adding/changing env vars or build defines** — update Build Configuration > Environment Variables
- **Changing linting or style rules** — update Code Style
- **Integrating a new third-party service or SDK** — add to External Integrations
- **Bumping the version** — update the version in Project Overview
- **Adding/removing files** — update File Inventory
- **Finding a new limitation** — add to Known Issues

### Supplementary docs
For sections that grow large, move them to separate files under `docs/` and link from here.

### Future improvements tracking
When a new feature is added and related enhancements or follow-up ideas are suggested but declined, add them as `- [ ]` items to `docs/CLAUDE.md/future-improvements.md`. This preserves good ideas for later without cluttering the current task.

### Version history maintenance
See `docs/CLAUDE.md/version-history.md`.

### Testing checklist maintenance
When adding or modifying user-facing behavior (new settings, UI modes, protocol commands, or display changes), add corresponding `- [ ]` test items to `docs/CLAUDE.md/testing-checklist.md`. Each item should describe the expected observable behavior, not the implementation detail.

### What belongs here vs. in code comments
- **Here:** Architecture decisions, cross-cutting concerns, "how things fit together," gotchas, recipes
- **In code:** Implementation details, function-level docs, inline explanations of tricky logic

---

## Origin

Created with Claude (Anthropic)
