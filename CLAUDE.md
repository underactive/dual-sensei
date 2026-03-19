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
- `src/display.h/.cpp` — U8g2 SH1106 OLED driver, screen rendering, ControllerState struct
- `src/input.h/.cpp` — ISR-driven encoder + button input, FreeRTOS event queue
- `src/menu.h/.cpp` — Menu state machine + NVS settings persistence

### Dependencies
- **PlatformIO** — Build system
- **Arduino-ESP32** — Framework (built on ESP-IDF internally; all IDF APIs still accessible via direct includes)
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
- GPIO ISR service serializes both encoder channel handlers on single core

#### 2. Display System (`display.h/.cpp`)
- U8g2 full-buffer mode (`_F_`) — 1KB RAM for 128x64 framebuffer
- Screen types: SPLASH, PAIRING, VISUALIZER, MENU
- `display_update()` throttled to ~15 FPS via `millis()` check
- Splash is one-shot (rendered once, not redrawn)
- Menu rendering queries `menu.h` getters — no circular header dependency
- **Visualizer screen**: PS1 controller layout with D-pad, face buttons (△○×□), shoulders (L1/L2/R1/R2), Select/Start — all driven by `ControllerState` struct. Shows live PS1 active-low protocol bytes at bottom. In Phase 1, all buttons show as released ("FF FF"). Epoch 2 populates state from DualSense HID.
- **ControllerState**: Struct in `display.h` with 14 button bools + `connected` flag, passed via `display_set_controller()`

#### 3. Menu System (`menu.h/.cpp`)
- State machine: HOME → SETTINGS → SETTING_EDIT / PAIRING / ABOUT
- Encoder navigates items; CON enters/confirms; BAK goes back/discards
- Settings saved to NVS on CON, discarded on BAK (snapshot/restore pattern)
- Three persistent settings: trigger threshold (0-255), stick-to-dpad (bool), player number (1-2)
- Five menu items: Trigger Thresh, Stick→DPad, Player Number, Pairing, About

#### 4. Settings / Configuration Storage
```
NVS namespace: "dual-sensei"
Keys:
  "trig_thresh" — uint8_t (default 128)
  "stick_dpad"  — bool (default false)
  "player_num"  — uint8_t (default 1)
```
- Saved to ESP32 NVS via Arduino Preferences library
- Loaded on boot in `menu_init()` with range validation (player_num clamped to 1-2)

---

## Build Configuration

### PlatformIO Configuration
- **`framework = arduino`** — Arduino-ESP32 (built on ESP-IDF internally). All ESP-IDF APIs accessible via direct includes (`driver/gpio.h`, `driver/spi_slave.h`, etc.). Avoids the managed component bloat of `framework = espidf, arduino` which caused build failures during Epoch 1 scaffolding.
- **`board_build.partitions = partitions.csv`** — Custom partition table with ~1.9MB app partition (BT Classic stack is large)
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

### Bluepad32 (Epoch 2 — not yet integrated)
- **What:** DualSense Bluetooth HID host library for ESP32
- **Loaded via:** PlatformIO `lib_deps` (to be added in Epoch 2)
- **Lifecycle:** Initializes BT Classic stack, manages pairing and connection callbacks
- **Key env vars:** Requires BT Classic enabled in sdkconfig (`CONFIG_BT_CLASSIC_ENABLED=y` — already configured in `sdkconfig.defaults`)
- **Gotchas:** BT Classic stack is large (~1MB); custom partition table with 1.9MB app space is required. Arduino-ESP32 enables BT Classic by default, but Bluepad32 may need additional sdkconfig overrides via `build_flags`.

---

## Known Issues / Limitations

1. **EC11 encoder detent count** — `ENC_STEPS_PER_DETENT` is set to 4 (most common). If the display board's encoder has a different ratio, navigation will feel too fast or too slow. Adjust in `config.h`.
2. **GPIO ISR service double-install** — Arduino-ESP32 may pre-install the GPIO ISR service. `input_init()` tolerates `ESP_ERR_INVALID_STATE` from `gpio_install_isr_service()`.
3. **I2C display blocking** — `u8g2.sendBuffer()` blocks the main loop for ~20ms per frame. Acceptable in Phase 1, but Phase 3's SPI slave has hard real-time constraints. Will need display rendering in a separate FreeRTOS task.
4. **No unit tests yet** — Testable pure-logic functions exist (encoder table, menu state machine, value formatting) but test infrastructure is deferred to Epoch 3.
5. **sdkconfig.defaults not auto-applied** — With `framework = arduino`, the sdkconfig.defaults file is a reference document only. Arduino-ESP32 has its own built-in sdkconfig. To override IDF settings, use `build_flags` in `platformio.ini`.

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
1. `src/config.h` — `FW_VERSION` define
2. `CLAUDE.md` — Project Overview section

**Keep all version references in sync.**

### Add a new NVS setting
1. Add default define in `src/config.h`
2. Add variable + getter in `src/menu.h` / `src/menu.cpp`
3. Add NVS load in `menu_init()` with range validation
4. Add menu item in `MENU_LABELS[]`, increment `EDITABLE_COUNT` if editable
5. Add edit handling in `handle_edit()`
6. Add format case in `format_setting_value()`
7. Add save/discard/snapshot cases

### Add a new display screen
1. Add enum value in `src/display.h` `Screen` enum
2. Add render function `render_xxx()` in `src/display.cpp`
3. Add case to `display_update()` switch
4. If data-driven, add setter functions in `display.h`

### Add a new menu action item (non-editable)
1. Add label to `MENU_LABELS[]` in `src/menu.cpp` (after editable items, before About)
2. Add case to `handle_settings()` CON handler for entering the new state
3. Add new `MenuState` enum value in `src/menu.h` if it has its own screen
4. Add handler function (e.g., `handle_xxx()`) with BAK → return to settings
5. Add dispatch case in `menu_handle_input()`

---

## File Inventory

| File / Directory | Purpose |
|------------------|---------|
| `src/main.cpp` | Entry point: setup()/loop() orchestration |
| `src/config.h` | Pin definitions, constants, thresholds |
| `src/display.h/.cpp` | U8g2 SH1106 OLED driver + screen rendering + ControllerState struct |
| `src/input.h/.cpp` | ISR encoder + button input, FreeRTOS queue |
| `src/menu.h/.cpp` | Menu state machine + NVS settings |
| `platformio.ini` | PlatformIO build configuration |
| `sdkconfig.defaults` | ESP-IDF SDK defaults reference (not auto-applied with Arduino framework) |
| `partitions.csv` | Custom flash partition table (1.9MB app, 20KB NVS, 64KB SPIFFS) |
| `Makefile` | Build shortcuts (`make build`, `make flash`, `make monitor`, `make clean`) |
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
