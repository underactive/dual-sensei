# Implementation: Epoch 2 — Bluepad32 DualSense Integration

## Files Changed

- `platformio.ini` — Switched from `framework = arduino` to `framework = espidf`, pioarduino platform, added `src_dir = main`, `lib_compat_mode = off`, `embed_txtfiles`
- `CMakeLists.txt` — Updated to ESP-IDF project format
- `sdkconfig.defaults` — Full rewrite for BTstack/Bluepad32/Arduino component config
- `.gitignore` — Added `components/`, `sdkconfig.esp32`, `sdkconfig.esp32dev`
- `main/main.c` — New: BTstack/Bluepad32 bootstrapper (from official template)
- `main/CMakeLists.txt` — New: ESP-IDF component registration for all source files
- `main/bt.h` — New: Bluetooth subsystem API
- `main/bt.cpp` — New: Bluepad32 integration, DualSense → ControllerState mapping
- `main/main.cpp` — Added `bt_init()`, `bt_update()`, `bt_get_state()` calls, `vTaskDelay(1)`
- `main/menu.cpp` — Added `bt_start_pairing()`/`bt_stop_pairing()` calls in pairing handlers
- `main/config.h` — Version bump to 0.2.0
- `main/display.cpp` — Moved from src/ (no code changes)
- `main/display.h` — Moved from src/ (no code changes)
- `main/input.cpp` — Moved from src/ (no code changes)
- `main/input.h` — Moved from src/ (no code changes)
- `main/menu.h` — Moved from src/ (no code changes)
- `CLAUDE.md` — Major update: architecture, dependencies, build config, file inventory, BT subsystem docs
- `docs/CLAUDE.md/testing-checklist.md` — Added Epoch 2 testing items
- `src/` — Removed (all files moved to `main/`)
- `components/` — Added: arduino, bluepad32, bluepad32_arduino, btstack, cmd_nvs, cmd_system (from template, gitignored)

## Summary

Successfully migrated from `framework = arduino` to `framework = espidf` with Arduino as a component, using the official esp-idf-arduino-bluepad32-template. Key challenges:

1. **sdkconfig handling**: PlatformIO generates `sdkconfig.esp32` from `sdkconfig.defaults`. If stale `sdkconfig.esp32` exists, it overrides defaults (caused Bluedroid/BTstack symbol collision).
2. **U8g2 compatibility**: Required `lib_compat_mode = off` since PlatformIO doesn't recognize U8g2 as ESP-IDF-compatible.
3. **Managed components**: Arduino component pulls in ESP Insights/Rainmaker as transitive deps, requiring `embed_txtfiles` for their certificate files even though we don't use them.

DualSense mapping in `bt.cpp`:
- Face buttons via Bluepad32's Xbox naming convention (A→Cross, B→Circle, X→Square, Y→Triangle)
- Analog triggers (0-1023) thresholded against `trigger_threshold` setting (0-255, scaled `/4`)
- Stick-to-DPad with 128-unit deadzone (~25% of ±512 range), OR'd with physical D-pad

## Verification

- `pio run` — builds successfully (86KB RAM / 774KB Flash)
- Hardware verification pending: flash and test with DualSense

## Follow-ups

- Flash and verify on hardware (all items in Epoch 2 testing checklist)
- Post-implementation audit (7 subagents per CLAUDE.md process)
- Consider adding BT connection status to the visualizer status line (show "Pairing..." vs "Connected")
- LED indicator for BT connection state (PIN_LED_BT)
