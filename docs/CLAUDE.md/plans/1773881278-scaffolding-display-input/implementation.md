# Epoch 1 Implementation: Project Scaffolding + Display + Input

## Files Changed

| File | Action | Description |
|------|--------|-------------|
| `platformio.ini` | Created | Arduino framework (not espidf,arduino — avoids managed component bloat), U8g2 lib_deps, custom partitions |
| `sdkconfig.defaults` | Created | Reference config for ESP-IDF settings (not auto-applied with Arduino framework) |
| `partitions.csv` | Created | Custom partition: ~1.9MB app, 20KB NVS, 64KB SPIFFS |
| `src/config.h` | Created | All GPIO pins, display/input/protocol constants, NVS namespace |
| `src/main.cpp` | Created | Arduino setup()/loop() — initializes display, input, menu; orchestrates event loop |
| `src/display.h` | Created | Screen enum, display API |
| `src/display.cpp` | Created | U8g2 SH1106 128x64 full-buffer, 5 screen renderers (splash, pairing, visualizer, settings, about) |
| `src/input.h` | Created | InputEvent enum, input API (init, poll, encoder position) |
| `src/input.cpp` | Created | Quadrature encoder ISR with 16-entry state machine, 3 button ISRs with 50ms debounce, FreeRTOS queue |
| `src/menu.h` | Created | MenuState enum, menu API, settings getters |
| `src/menu.cpp` | Created | 5-state menu machine (HOME/SETTINGS/EDIT/PAIRING/ABOUT), NVS persistence via Preferences |
| `CLAUDE.md` | Created | Full project context doc from template |
| `.gitignore` | Created | PlatformIO, ESP-IDF, IDE, macOS ignores |

## Summary

Implemented the full Epoch 1 scope: project scaffolding through menu system.

**Deviation from plan:** Changed from `framework = espidf, arduino` to `framework = arduino`. The dual-framework setup pulled in ~25 managed ESP-IDF components (Rainmaker, Zigbee, Modbus, etc.) via the Arduino core's idf_component.yml, causing build failures (missing HTTPS certificates). Since Arduino-ESP32 is built on ESP-IDF internally, all IDF APIs (gpio driver, SPI slave, FreeRTOS task pinning) remain accessible via direct includes. This is the simpler, more maintainable approach.

**Deviation from plan:** Removed the `main/` directory and `main/CMakeLists.txt`. With PlatformIO, all source goes in `src/`. The `main/` split was an ESP-IDF native convention that doesn't apply.

## Verification

- `pio run` succeeds: firmware builds in ~13s
- RAM: 23.5KB / 327KB (7.2%) — ~304KB available for BT stack in Epoch 2
- Flash: 364KB / 1.9MB (18.5%) — ample room for remaining epochs
- No compiler warnings in project source code

## Follow-ups

- **Hardware verification needed:** Encoder `ENC_STEPS_PER_DETENT` (default 4) may need adjustment for the specific display board
- **Epoch 2:** Bluepad32 Arduino library integration, DualSense pairing, input visualizer with BT data
- **Epoch 3:** Button mapper, SPI slave protocol, debug LEDs
- **I2C blocking:** Display `sendBuffer()` blocks main loop for ~20ms. Will need a separate FreeRTOS task before Phase 3 SPI slave integration

## Audit Fixes

### Fixes Applied

1. **input.cpp — ISR null-guard on queue handle** (Security #2, #3): Added `if (!input_queue) return;` at top of both `encoder_isr` and `button_isr`. Added null-check with `abort()` after `xQueueCreate`.
2. **input.cpp — Button ISR bounds check** (Security #1): Added `if (idx >= BTN_COUNT) return;` guard in `button_isr`.
3. **input.cpp — uintptr_t cast** (QA #6): Changed `(uint32_t)arg` to `(uint32_t)(uintptr_t)arg` and `(void*)N` to `(void*)(uintptr_t)N`.
4. **input.cpp — Re-init safety** (Resource #5, #6): Added cleanup prologue to `input_init()` — removes ISR handlers and deletes queue if called more than once.
5. **input.cpp — gpio_install_isr_service fatal on real failure** (Interface #2): Changed to `abort()` on errors other than `ESP_ERR_INVALID_STATE`.
6. **input.cpp — Encoder ISR serialization documented** (Resource #2): Added concurrency note explaining GPIO ISR service multiplexing.
7. **input.cpp — WHY comment on driver/gpio.h include** (DX #11): Documented why ESP-IDF API is used instead of Arduino `attachInterrupt`.
8. **menu.cpp — NVS player_number validation** (Security #6, QA #4): Added `if (player_number < PLAYER_NUM_MIN || player_number > PLAYER_NUM_MAX)` check after NVS load.
9. **menu.cpp — prefs.begin error check** (Security #7): Added return value check with warning log.
10. **menu.cpp — Named menu item indices** (DX #7): Replaced magic `3`/`4` with `MENU_IDX_PAIRING`/`MENU_IDX_ABOUT`.
11. **menu.cpp — Named player number range** (DX #8): Added `PLAYER_NUM_MIN`/`PLAYER_NUM_MAX` constants.
12. **menu.cpp — MENU_ABOUT screen state documented** (QA #1, #2, State #5): Added comments explaining SCREEN_MENU is already set when entering/exiting About.
13. **menu.h — Added menu_get_editable_count()** (QA #3): Exposed `EDITABLE_COUNT` via getter to eliminate magic number in display.cpp.
14. **menu.h — "Epoch 3" → "Phase 3"** (DX #6): Terminology consistency.
15. **display.cpp — Named layout constants** (DX #2, #4): Moved `MAX_VISIBLE`, `ITEM_H`, `LIST_Y` to file-scope named constants. Added `SETTINGS_VAL_COL`/`SETTINGS_VAL_EDIT_COL`.
16. **display.cpp — menu_get_editable_count()** (QA #3, DX #3): Replaced hardcoded `3` with `menu_get_editable_count()`.
17. **display.cpp — decorated buffer sizing** (Security #4): Changed to `sizeof(val) + 3` derived from source buffer.
18. **display.cpp — scroll_offset documentation** (QA #8, DX #6): Added comment explaining self-correction behavior.
19. **main.cpp — Guarded visualizer data push** (QA #12, State #8, Resource #8): Wrapped `digitalRead` calls and display setters with `display_get_screen() == SCREEN_VISUALIZER` check.

### Verification Checklist

- [x] Build succeeds after all fixes (`pio run` — SUCCESS, 6.2s)
- [ ] Encoder input still produces CW/CCW events on hardware
- [ ] Button presses still produce events (debounce working)
- [ ] Menu navigation functions correctly (enter/exit/edit/save/discard)
- [ ] Settings persist after power cycle
- [ ] No crashes during 10-minute endurance test

### Deferred Items

- **NVS write error checking** (Interface #6): `Preferences::putUChar` return value unchecked. Accepted — ESP32 NVS is reliable for single-user writes.
- **Preferences handle never closed** (Resource #7): Acceptable for single-namespace single-owner device.
- **Display↔menu circular coupling** (State #7): Accepted at Phase 1 scale; thin interfaces keep it manageable.
- **Button state dual source of truth** (State #3): `digitalRead` for visualizer vs. ISR queue for logic. Intentional — visualizer shows raw GPIO for hardware debugging.
- **I2C blocking main loop** (Resource #9): ~20ms per frame blocks `input_poll`. Acceptable for Phase 1; Phase 3 will need display task separation.
- **Unit tests** (Testing #1-14): Deferred to Epoch 3 when test infrastructure is created. Menu state machine and `ENC_TABLE` are the highest-priority testable targets.
