# Implementation: PS2 Scope Expansion

## Files Changed

| File | Change |
|------|--------|
| `main/config.h` | Renamed `PIN_PS1_*` → `PIN_PSX_*`, added `PS2_DEVICE_ID_ANALOG`, `PS2_STICK_CENTER`, `PSX_DATA_MARKER`, `PSX_IDLE_BYTE`, `CONSOLE_MODE_DEFAULT` |
| `main/display.h` | Added `l3`, `r3`, `lx`, `ly`, `rx`, `ry` to `ControllerState`; updated struct comment |
| `main/bt.cpp` | Added `axis_to_ps2()` helper; mapped `thumbL()`/`thumbR()` → L3/R3; mapped stick axes to PS2 0-255 range |
| `main/menu.h` | Added `menu_get_console_mode()` declaration |
| `main/menu.cpp` | Full 8-point recipe for `console_mode` setting (SID=4, NVS key `con_mode`); updated `IDX_PAIRING`/`IDX_ABOUT` (+1); updated Player Number help text |
| `main/display.cpp` | Added `draw_analog_stick()`; replaced `compute_ps1_bytes()` → `compute_protocol_bytes()` (PS1: 2 bytes, PS2: 6 bytes); redesigned visualizer layout with compressed buttons + stick circles; updated about screen text |
| `CLAUDE.md` | Updated project overview, pin table, ControllerState docs, settings section, NVS table, visualizer description |

## Summary

Implemented all changes per plan without deviations. The visualizer layout was compressed to fit analog stick circles below the D-pad and face buttons:
- Shoulders moved up (y=18 from y=21)
- D-pad center moved up (y=32 from y=38)
- Face button radius reduced (4 from 5)
- Stick circles at y=50 with r=5, L3/R3 labels alongside
- Protocol bytes show PS1 (2 bytes) or PS2 (6 bytes) based on console mode setting

## Verification

1. `pio run` — builds cleanly (SUCCESS)
2. RAM: 26.3%, Flash: 39.5% — no significant size increase
3. Hardware verification pending (flash + manual testing)

## Follow-ups

- Hardware test: verify stick circles render correctly on SH1106 OLED
- Hardware test: verify DualSense stick movement reflected in real-time
- Hardware test: verify L3/R3 press fills stick circle
- Hardware test: verify console mode setting persists across reboot

## Audit Fixes

### Fixes applied

1. **`axis_to_ps2()` signed right-shift** — Changed `>> 2` to `/ 4` to avoid implementation-defined behavior for signed integers pre-C++20 (QA-1)
2. **`count` uninitialized** — Initialized `uint8_t count = 0` before `compute_protocol_bytes()` call to prevent UB if function is modified without updating all branches (QA-2 / Interface-1)
3. **L3/R3 comment accuracy** — Updated comment to clarify PS1 bits 1,2 are "always 1 (unused per digital pad spec)" instead of misleading "unused" (QA-4 / Interface-4)
4. **`SID_CONSOLE_MODE` alignment** — Fixed indentation in 8 locations across `menu.cpp` to match peer case alignment (DX-H1)
5. **`console_mode` symmetric validation** — Changed one-sided `> CONSOLE_MODE_MAX` check to symmetric `< CONSOLE_MODE_MIN || > CONSOLE_MODE_MAX` for consistency with `player_number` validation pattern (Security-4)
6. **Protocol bytes comment** — Added inline comment `(PS1: 2 button bytes, PS2: 2 button + 4 stick bytes)` at call site (DX-M3)
7. **Testing checklist** — Added 18 new checklist items for PS2 features (Testing-9)

### Verification checklist

- [x] `pio run` builds cleanly after all fixes
- [ ] Verify `axis_to_ps2(0)` produces 128 (center) on hardware — stick centered at rest
- [ ] Verify `axis_to_ps2(512)` clamps to 255 (max) — full right/down deflection
- [ ] Verify console mode NVS validation by corrupting NVS and rebooting — should default to PS1
- [ ] Verify all SID_CONSOLE_MODE cases align visually in code review

### Unresolved items

- **Testing HIGH-1,2,3**: No unit tests for `compute_protocol_bytes`, `axis_to_ps2`, or menu edit handlers. Deferred to Epoch 3 test infrastructure (Known Issue #4). These are pure functions that can be wrapped in host-side tests.
- **DX-M1**: Protocol logic in display layer will be extracted to a shared module when Phase 3 SPI slave is implemented.
- **State-1/Resource-1,2**: Dual ControllerState copies without mutex. Safe today (single task). Known Issue #3 tracks the Phase 3 display task extraction that will require synchronization.
