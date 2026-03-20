# Implementation: Multi-Controller Support (DS4 + Xbox One + Switch Pro)

## Files changed

- `main/bt.h` — Added `bt_get_controller_name()` declaration, updated `bt_play_rumble()` comment
- `main/bt.cpp` — Added `controller_name[16]` buffer, populated from `ctl->getModelName()` in `on_connected()`, cleared in `on_disconnected()`, implemented `bt_get_controller_name()` getter
- `main/display.cpp` — Updated `render_pairing()` with multi-controller instructions (DS4/5, Xbox, Switch), updated `render_visualizer()` status line to show controller type name via `bt_get_controller_name()`
- `main/menu.cpp` — Updated Pairing help text from "Pair DualSense controller" to "Pair wireless controller"
- `CLAUDE.md` — Updated Project Overview, Bluetooth System section, Display System section, External Integrations / Bluepad32 section
- `README.md` — Updated title, description, diagram, supported controllers list, status checklist
- `docs/CLAUDE.md/testing-checklist.md` — Added "Multi-Controller Support" section with 20 test items
- `docs/CLAUDE.md/version-history.md` — Updated v0.2.0 entry to include multi-controller support
- `docs/CLAUDE.md/future-improvements.md` — Updated touchpad item (pre-existing fix from earlier in session)

## Summary

Implemented exactly as planned. No deviations. The key insight confirmed during implementation: zero changes needed to `map_controller_data()`, rumble, trigger thresholds, stick-to-dpad, or any data path code. Bluepad32's unified API handles all controller differences transparently. The entire implementation was UI text, a 4-line getter API, and documentation.

### Code changes
- **bt.cpp**: 3 additions — `controller_name[16]` buffer, `snprintf` in `on_connected()` to copy from `ctl->getModelName()`, clear in `on_disconnected()`, and `bt_get_controller_name()` returning the buffer
- **display.cpp**: Pairing screen switched from `u8g2_font_6x10_tr` to `u8g2_font_5x7_tr` for instruction lines to fit 3 rows (DS4/5, Xbox, Switch). Visualizer calls `bt_get_controller_name()` instead of hardcoded "Connected".
- **menu.cpp**: Single string change in `MENU_ITEMS[]`

## Verification

- Build succeeds: `[SUCCESS]` via `make build`
- RAM: 26.3% (86,308 / 327,680 bytes) — unchanged from before
- Flash: 39.5% (776,880 / 1,966,080 bytes) — unchanged from before
- No new compiler warnings

## Follow-ups

- Physical hardware testing required for DS4, Xbox One, and Switch Pro (only DualSense tested so far)
- Xbox firmware v5.x BLE pairing untested on this hardware
- DS4 touchpad virtual mouse untested (should work per Bluepad32 docs but needs verification)
- Switch Pro auto-reconnect after power cycle untested

## Audit Fixes

### Fixes applied

1. **Null guard on `model.c_str()`** — Added null check before passing to `snprintf` in `bt.cpp:on_connected()`. Addresses Security Audit finding: Arduino `String::c_str()` could return nullptr on OOM. Now falls back to `"Controller"`.
2. **`bt_play_rumble()` parameter documentation** — Added inline doc comment in `bt.h` specifying motor intensity range (0-255) and no-op behavior when disconnected. Addresses DX Audit finding.
3. **Missing testing checklist items** — Added 3 items to Epoch 2 BT section: touchpad virtual device serial log, rumble after reconnect, pairing screen visual transition. Addresses Testing Coverage Audit findings.

### Verification checklist

- [ ] Build succeeds after audit fixes (`make build`)
- [ ] Null guard: if `getModelName()` returned empty, display shows "Controller" fallback
- [ ] `bt_play_rumble` doc comment visible in `bt.h`
- [ ] Testing checklist contains touchpad virtual device, rumble reconnect, and pairing transition items

### Unresolved items

The remaining 13 warnings are all pre-existing patterns not introduced by this change:
- NVS open failure silent writes (interface contract) — existing design, acceptable for embedded
- `connected_gamepad` not volatile (state management) — safe due to single-task execution model
- I2C display blocking (resource) — documented in Known Issues #3
- Face button draw boilerplate (DX) — pre-existing display code
- PSX protocol magic numbers (DX) — pre-existing, candidate for Phase 3 extraction
- `compute_protocol_bytes()` in display layer (DX) — pre-existing, will extract for Phase 3 SPI
- `controller_name` buffer truncation for unknown future controllers (QA) — 16 bytes fits all known names
- `bt_get_controller_name()` pointer lifetime (interface contract) — safe under current single-task model
