# Epoch 1: Project Scaffolding + Display + Input

## Objective

Set up the PlatformIO project with ESP-IDF + Arduino as component, implement the SH1106 OLED display driver, ISR-driven rotary encoder + button input, and a menu state machine with NVS persistence. This epoch validates the local UI hardware (OLED, encoder, buttons) independently of Bluetooth or PS1 protocol.

## Changes

| File | Action | Description |
|------|--------|-------------|
| `platformio.ini` | Create | ESP-IDF + Arduino framework, U8g2 lib dep, partition table ref |
| `sdkconfig.defaults` | Create | BT Classic enabled (prep for Epoch 2), BLE off, 240MHz, SPI IRAM |
| `partitions.csv` | Create | Custom partition: ~1.9MB app (room for BT stack), 64KB SPIFFS |
| `src/config.h` | Create | All GPIO pin definitions, display/input/protocol constants |
| `src/main.cpp` | Create | Arduino setup()/loop() entry point, orchestrates subsystems |
| `src/display.h` | Create | Screen enum, display API (init, splash, screen switch, update) |
| `src/display.cpp` | Create | U8g2 SH1106 driver, screen renderers (splash, pairing, visualizer, menu) |
| `src/input.h` | Create | InputEvent enum, input API (init, poll, encoder position) |
| `src/input.cpp` | Create | ISR-driven quadrature encoder (state-machine debounce), button debounce via esp_timer_get_time(), FreeRTOS event queue |
| `src/menu.h` | Create | MenuState enum, menu API (init, handle_input, state/settings getters) |
| `src/menu.cpp` | Create | Menu state machine (HOME/SETTINGS/EDIT/PAIRING/ABOUT), NVS via Preferences |
| `CLAUDE.md` | Create | Project context doc populated from template |
| `.gitignore` | Create | PlatformIO, ESP-IDF, IDE ignores |

## Dependencies

- Steps are sequential within the epoch: config.h first (all others depend on pin defs), then display/input (independent of each other), then menu (depends on display for screen switching).
- U8g2 library pulled via PlatformIO lib_deps — no manual download needed.
- Bluepad32 is NOT included in this epoch (Epoch 2).

## Risks / Open Questions

1. **EC11 encoder steps-per-detent**: Unknown for the specific display board. Defaulting to 4 transitions per detent; may need adjustment.
2. **U8g2 HW I2C on ESP-IDF+Arduino**: Should work with default I2C pins (SDA=21, SCL=22) but verify the constructor pins are respected.
3. **GPIO ISR service conflict**: Arduino-ESP32 may pre-install the GPIO ISR service; using `gpio_install_isr_service()` with error tolerance.
4. **`framework = espidf, arduino` compatibility**: PlatformIO supports this but it can be fragile with version mismatches. Pin platform version if issues arise.
