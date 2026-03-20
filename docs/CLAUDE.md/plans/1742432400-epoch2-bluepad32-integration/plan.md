# Plan: Epoch 2 — Bluepad32 DualSense Integration

## Objective

Integrate Bluepad32 v4.2.0 for DualSense wireless connectivity. Migrate the project from `framework = arduino` to `framework = espidf` (with Arduino as a component) using the official esp-idf-arduino-bluepad32-template. Connect DualSense button/stick/trigger data to the existing ControllerState struct and visualizer.

---

## Changes

### 1. Project structure migration

**Move source files:**
- `src/*.cpp` + `src/*.h` → `main/` (PlatformIO `src_dir = main` for espidf)
- Keep `main.c` (BTstack/Bluepad32 bootstrapper) from template as-is

**Add components (git submodules):**
- `components/arduino` — Arduino-ESP32 core (from template)
- `components/bluepad32` — Bluepad32 library
- `components/bluepad32_arduino` — Arduino API wrapper
- `components/btstack` — BTstack Bluetooth host
- `components/cmd_nvs`, `components/cmd_system` — Console commands (required by Bluepad32)

**New files:**
- `main/main.c` — BTstack bootstrap (from template, unmodified)
- `main/CMakeLists.txt` — ESP-IDF component registration listing all .cpp/.c sources
- `CMakeLists.txt` — Top-level ESP-IDF project file

**Update build config:**
- `platformio.ini` — Switch to `framework = espidf`, pioarduino platform, `src_dir = main`
- `sdkconfig.defaults` — Replace with template's config, add our custom settings, disable Bluepad32 console (`CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE=n`) to keep `Serial`

### 2. New module: `bt.h` / `bt.cpp` — Bluetooth subsystem

**Purpose:** Encapsulate all Bluepad32 interaction behind a C-style API matching our module pattern.

**API:**
```c
void bt_init();                          // Call BP32.setup() with callbacks
void bt_update();                        // Call BP32.update(), map data to ControllerState
bool bt_is_connected();                  // Check if a controller is connected
const ControllerState& bt_get_state();   // Get current mapped controller state
void bt_start_pairing();                 // Enable new BT connections (scanning)
void bt_stop_pairing();                  // Disable scanning
void bt_forget_keys();                   // Clear stored BT pairings
```

**DualSense → ControllerState mapping:**
- D-pad: `ctl->dpad() & DPAD_UP/DOWN/LEFT/RIGHT`
- Face buttons: `ctl->a()` → cross, `ctl->b()` → circle, `ctl->x()` → square, `ctl->y()` → triangle
- Shoulders: `ctl->l1()`, `ctl->r1()`
- Triggers: `(ctl->brake() / 4) >= trigger_threshold` → L2, `(ctl->throttle() / 4) >= trigger_threshold` → R2
- Select/Start: `ctl->miscSelect()`, `ctl->miscStart()`
- Stick-to-DPad: if enabled, OR left stick axes (with deadzone) into d-pad bools

### 3. Update `main.cpp` — Wire bt module into loop

```cpp
void setup() {
    // ... existing init ...
    bt_init();
}

void loop() {
    InputEvent evt = input_poll();
    if (evt != INPUT_NONE) menu_handle_input(evt);

    bt_update();  // Poll Bluepad32, map controller data

    if (display_get_screen() == SCREEN_VISUALIZER) {
        display_set_controller(bt_get_state());
    }

    display_update();
    vTaskDelay(1);  // Yield to avoid WDT (required with espidf framework)
}
```

### 4. Update menu pairing flow

- `handle_pairing` entry → call `bt_start_pairing()`
- `handle_pairing` exit (BAK) → call `bt_stop_pairing()`
- Display pairing screen shows connection status from `bt_is_connected()`

### 5. Update `sdkconfig.defaults`

Key settings from template:
- `CONFIG_BT_ENABLED=y`, `CONFIG_BT_CONTROLLER_ONLY=y`
- `CONFIG_BTDM_CTRL_MODE_BTDM=y` (dual-mode BT)
- `CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE=n` (keep Serial working)
- `CONFIG_AUTOSTART_ARDUINO=n` (main.c handles bootstrap)
- `CONFIG_FREERTOS_HZ=1000`
- `CONFIG_PARTITION_TABLE_CUSTOM=y` (use our custom partitions.csv)

### 6. Update `.gitignore`

- Add `sdkconfig` (generated, should not be tracked)
- Add `managed_components/` (ESP-IDF managed components cache)

### 7. Remove old `src/` directory

After confirming build succeeds with files in `main/`, delete the empty `src/` directory.

---

## Dependencies

1. Components must be added before build config changes (CMakeLists needs them)
2. `main.c` must exist before `platformio.ini` switches to espidf (build would fail without entry point)
3. `bt.h/bt.cpp` depends on Bluepad32 headers from components
4. `sdkconfig.defaults` must be updated before first build (BTstack config is mandatory)

## Risks / Open Questions

1. **Binary size**: BTstack + Bluepad32 + Arduino + U8g2 may exceed 1.9MB app partition. Verify after first build; increase partition if needed.
2. **Serial compatibility**: Disabling Bluepad32 console (`CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE=n`) should restore normal Serial. Verify.
3. **U8g2 with espidf framework**: Should work via `lib_deps` with pioarduino platform. Verify.
4. **Preferences library**: Arduino `Preferences` should still work as an Arduino component. Verify NVS still persists.
5. **vTaskDelay requirement**: The Arduino loop must yield periodically with espidf framework to avoid WDT. 1ms delay is minimal.
6. **Stick deadzone**: Need a threshold constant for stick-to-dpad mapping. Use 25% of range (~128 out of 512) as default.
