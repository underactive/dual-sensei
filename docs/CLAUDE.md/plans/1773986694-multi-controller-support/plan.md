# Plan: Multi-Controller Support (DS4 + Xbox One + Switch Pro)

## Objective

Add support for PS4 DualShock 4, Xbox One (Bluetooth models), and Nintendo Switch Pro controllers alongside the existing DualSense. Bluepad32's unified gamepad API already handles all data mapping — the entire button/trigger/stick/rumble pipeline works identically across controller types with zero code changes. This plan covers the **UI, messaging, and documentation** changes needed to make the experience controller-agnostic.

### Why this is low-risk

Bluepad32 v4.2.0 parses each controller type through its own HID parser (`uni_hid_parser_ds4`, `uni_hid_parser_ds5`, `uni_hid_parser_xboxone`, `uni_hid_parser_switch`) and populates a single unified `uni_gamepad_t` struct. The Arduino wrapper (`Controller` class) exposes identical methods regardless of controller type: `a()`, `b()`, `dpad()`, `axisX()`, `brake()`, `throttle()`, `playDualRumble()`, etc. Button naming is positional (A=south=Cross, B=east=Circle), so PS1/PS2 output mapping is correct for all four controllers.

### Controller compatibility

| Controller | Protocol | ESP32-WROOM | Rumble | Sticks | Touchpad |
|-----------|----------|-------------|--------|--------|----------|
| DualShock 4 (PS4) | BR/EDR | Yes | Yes | Yes | Yes (virtual mouse) |
| DualSense (PS5) | BR/EDR | Yes | Yes | Yes | Yes (virtual mouse) |
| Xbox One (fw v3.x/v4.x) | BR/EDR | Yes | Yes | Yes | No |
| Xbox One (fw v5.x+) | BLE | Yes | Yes | Yes | No |
| Switch Pro | BR/EDR | Yes | Yes | Yes | No |

ESP32-WROOM-32 supports both BR/EDR and BLE — all controller variants work.

## Changes

### 1. `main/bt.h` — Add controller name getter, update comments

- Add `const char* bt_get_controller_name()` declaration
- Update `bt_play_rumble()` doc comment: "DualSense" → "connected controller"

### 2. `main/bt.cpp` — Store and expose controller name

- Add `static char controller_name[16]` buffer
- In `on_connected()`: copy short name from `ctl->getModelName()` into buffer (truncated to 15 chars). Bluepad32's `getModelName()` already returns display-friendly strings:
  - `"DualShock 4"` for PS4 (enum value 34)
  - `"DualSense"` for PS5 (enum value 45)
  - `"XBox One"` for Xbox (enum value 32)
  - `"Switch Pro"` for Switch Pro (enum value 38)
- In `on_disconnected()`: clear `controller_name[0] = '\0'`
- Implement `bt_get_controller_name()`: return `controller_name`
- Update serial log in `on_connected()`: already prints model name via `ctl->getModelName()`, no change needed

### 3. `main/display.cpp` — Update pairing screen for multi-controller

Replace `render_pairing()` waiting state:

**Before:**
```
Waiting for
DualSense...
──────────────────────
Hold CREATE + PS
to pair controller
```

**After:**
```
Waiting for
controller...
──────────────────────
PS4/5: Share+PS hold
Xbox: pair btn 3s
Switch: sync btn
```

Header stays `u8g2_font_6x10_tr`. Instructions switch to `u8g2_font_5x7_tr` to fit 3 lines below separator (y=38, y=48, y=58). DS4 (Share+PS) and DualSense (Create+PS) use the same gesture — "Share" is the universally recognized label.

Connected state stays unchanged ("Controller Connected!" + "Press [BAK] to return").

### 4. `main/display.cpp` — Show controller type on visualizer status line

Replace hardcoded `"Connected"` with `bt_get_controller_name()`:

**Before:** `"Connected"` / `"No Controller"`
**After:** `"DualShock 4"` / `"DualSense"` / `"XBox One"` / `"Switch Pro"` / `"No Controller"`

Falls back to `"Controller"` if name is empty (shouldn't happen, but defensive).

### 5. `main/menu.cpp` — Update Pairing help text

Line 20: `"Pair DualSense controller"` → `"Pair wireless controller"`

### 6. `CLAUDE.md` — Update documentation

- **Project Overview**: Mention DS4 + Xbox One + Switch Pro support
- **Architecture > Bluetooth System**: Note multi-controller support, `bt_get_controller_name()` API, controller compatibility table
- **Architecture > Display System > Visualizer screen**: Status line now shows controller type
- **Architecture > Display System > Pairing screen**: Multi-controller instructions
- **External Integrations > Bluepad32**: Add supported controller list, note unified API

### 7. `README.md` — Update description

- First paragraph: mention DS4, Xbox One, and Switch Pro alongside DualSense
- Status checklist: add multi-controller support line item

### 8. `docs/CLAUDE.md/testing-checklist.md` — Add multi-controller test items

New section "Multi-Controller Support" with items for:
- DS4 pairing + button/stick/trigger/touchpad/rumble verification
- Xbox One pairing + button/stick/trigger/rumble verification
- Switch Pro pairing + button/stick/rumble verification
- Controller name displayed correctly on visualizer for each type
- Pairing screen shows multi-controller instructions
- Touchpad Sel/St works with DS4 (has touchpad like DualSense)
- Touchpad Sel/St inactive (no crash) with Xbox/Switch
- Disconnect/reconnect cycle with different controller types

### 9. `docs/CLAUDE.md/version-history.md` — Add entry (at implementation time)

## Dependencies

- Changes 1-2 (bt API) must be done before changes 3-4 (display uses the new API)
- Change 5 (menu text) is independent
- Changes 6-9 (docs) are independent and can be done last

## Risks / Open Questions

1. **Xbox firmware version detection**: Bluepad32 handles BR/EDR vs BLE transparently, but different Xbox firmware versions may have subtle behavior differences (e.g., BLE rumble timing was adjusted in Bluepad32 v4.0.1). Only testable with physical hardware.

2. **`getModelName()` returns Arduino `String`**: This allocates on the heap. We copy into a fixed `char[16]` buffer in `on_connected()` (one-shot, not hot path) to avoid keeping the `String` alive. Per Development Rule #4 (avoid memory fragmentation in long-running code).

3. **Switch Pro reconnection**: Switch Pro may behave differently from DualSense for auto-reconnect after power cycle. BTstack stores pairing keys in NVS, so it should work, but this is untested.

4. **"Touchpad Sel/St" setting visibility**: Remains visible for all controller types. DS4 and DualSense both have touchpads — Bluepad32 exposes them as virtual mouse devices via `enableVirtualDevice(true)`, so the feature works with both. For Xbox/Switch, `connected_mouse` stays `nullptr` and the feature is safely inactive. No menu changes needed.

6. **DS4 touchpad virtual mouse**: DS4's touchpad should produce a virtual mouse device just like DualSense. The existing `connected_mouse` handling in `on_connected()` / `map_controller_data()` should work as-is — needs verification with physical hardware.

5. **Display string truncation**: Longest name is `"DualShock 4"` at 11 chars — fits comfortably in the status line alongside "P1"/"P2" on a 128px-wide display with 5x7 font (5px per char = ~25 chars). No truncation concern.
