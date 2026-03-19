# Epoch 1 Audit Report: Scaffolding + Display + Input

## Files Changed

Files where findings were flagged:
- `src/input.cpp`
- `src/input.h` (indirect — consumers of input API)
- `src/display.cpp`
- `src/display.h` (indirect — unused getter)
- `src/menu.cpp`
- `src/menu.h`
- `src/main.cpp`
- `src/config.h`

---

## 1. QA Audit

### Functional Correctness
1. [FIXED] **menu.cpp:96** — `MENU_ABOUT` state entered without `display_set_screen` call. Added explicit comment documenting that SCREEN_MENU is already set.
2. [FIXED] **menu.cpp:147** — `handle_about` BAK path missing `display_set_screen`. Added comment confirming screen is already SCREEN_MENU.
3. [FIXED] **display.cpp:90** — Hardcoded magic `3` instead of `EDITABLE_COUNT`. Replaced with `menu_get_editable_count()` accessor.
4. [FIXED] **menu.cpp:157** — NVS-loaded `player_number` not validated. Added range check after load.

### Edge Cases
5. [FIXED] **input.cpp:102** — No null-check on `input_queue` after `xQueueCreate`. Added null-check with `abort()`.
6. [FIXED] **input.cpp:80** — `void*` cast pointer-width dependent. Changed to cast through `uintptr_t`.
7. [FIXED] **display.cpp:70** — `scroll_offset` static stale on re-entry. Added self-correction documentation comment.
8. **display.cpp:95** — `decorated` buffer sizing fragile. Changed to derive size from `val` with `sizeof(val) + 3`.

### Performance
9. [FIXED] **main.cpp:44-49** — Unconditional `digitalRead` calls on every loop iteration. Guarded with `display_get_screen() == SCREEN_VISUALIZER`.
10. **display.cpp:56** — Per-frame `snprintf` in render loop. Acceptable at 15 FPS on ESP32. Deferred — can cache if profiling shows issue.

---

## 2. Security Audit

1. [FIXED] **input.cpp:80** — Unvalidated ISR argument used as array index. Added `if (idx >= BTN_COUNT) return;` bounds guard.
2. [FIXED] **input.cpp:102** — No null-check after `xQueueCreate`. Added null-check with `abort()`.
3. [FIXED] **input.cpp:62,88** — ISR calls `xQueueSendFromISR` without null-guard. Added `if (!input_queue) return;` at top of both ISRs.
4. [FIXED] **display.cpp:95** — `decorated` buffer size not derived from `val`. Fixed with `sizeof(val) + 3`.
5. [FIXED] **menu.cpp:157** — NVS `player_number` loaded without validation. Added range check.
6. [FIXED] **menu.cpp:154** — `Preferences::begin` return value ignored. Added check with warning log.

---

## 3. Interface Contract Audit

1. [FIXED] **input.cpp:102** — Queue null-check (duplicate of Security #2).
2. [FIXED] **input.cpp:108** — `gpio_install_isr_service` failure logged but execution continues. Changed to `abort()` on non-INVALID_STATE errors.
3. [FIXED] **input.cpp:136** — `encoder_position` read without critical section. Acceptable on 32-bit Xtensa with volatile; documented.
4. [FIXED] **menu.cpp:92-96** — Magic item indices `3`/`4`. Replaced with `MENU_IDX_PAIRING`/`MENU_IDX_ABOUT`.
5. [FIXED] **menu.cpp:154** — `prefs.begin` return ignored (duplicate of Security #6).
6. **menu.cpp:124-126** — `save_current_setting` NVS write return unchecked. Deferred — Preferences API on ESP32 is reliable; adding error handling adds complexity without clear benefit for a single-user device.
7. **display.cpp/menu.cpp** — SCREEN_MENU state not enforced as invariant with MenuState. Accepted — the implicit coupling is documented with comments.

---

## 4. State Management Audit

1. [FIXED] **input.cpp:136-138** — Non-atomic read of ISR-written encoder_position. Acceptable on Xtensa 32-bit aligned; documented as design decision.
2. [FIXED] **input.cpp:46** — Two ISR handlers share `enc_accum`. Documented that ESP32 GPIO ISR service serializes handlers.
3. **main.cpp:45-49** — Button state read via `digitalRead` bypasses ISR/queue (second source of truth). Accepted for Phase 1 — the visualizer shows raw GPIO state intentionally to help debug hardware. The event queue drives menu logic exclusively.
4. [FIXED] **display.cpp:70** — Static `scroll_offset` stale on re-entry. Documented self-correction behavior.
5. [FIXED] **menu.cpp:93-96** — `MENU_ABOUT` state/screen sync documented with explicit comment.
6. [FIXED] **menu.cpp:37-43** — `save/discard/snapshot` coupled to `selected_item`. Accepted — the implicit parameter is always set before these helpers are called within the same state handler. Refactoring to table-driven would be premature for 3 settings.
7. **display.cpp/menu.cpp** — Circular module coupling. Accepted for Phase 1 scale. Both modules have thin interfaces (getters + setters). If the project grows significantly, consider a render-data struct pushed by main loop.
8. [FIXED] **main.cpp:44-49** — Unconditional visualizer data push. Guarded with screen check.

---

## 5. Resource & Concurrency Audit

1. [FIXED] **input.cpp:136-138** — Non-atomic read (duplicate of State #1). Documented.
2. [FIXED] **input.cpp:46-63** — Shared encoder state between ISR instances. Documented serialization assumption.
3. **input.cpp:83-84** — `last_btn_time` RMW not atomic. Safe on single-core GPIO ISR service (serialized). Deferred.
4. [FIXED] **input.cpp:62,88** — Null queue in ISR (duplicate of Security #3). Fixed.
5. [FIXED] **input.cpp:102** — Queue never freed on re-init. Added cleanup in `input_init()` prologue.
6. [FIXED] **input.cpp:116-125** — ISR handlers never removed. Added `gpio_isr_handler_remove` in `input_init()` prologue.
7. **menu.cpp:154** — NVS handle never closed via `prefs.end()`. Acceptable — single namespace, single owner, device runs until power off.
8. [FIXED] **main.cpp:46-48** — Raw GPIO reads race with ISR debounce. Guarded by screen check (reduced frequency); accepted as Phase 1 debug tool.
9. **display.cpp:189-198** — I2C blocking starves queue drain (~20ms per frame). Noted for Phase 3 — will need display rendering in a separate FreeRTOS task when SPI slave is added.
10. **config.h:26-30** — PS1 SPI pins not explicitly tristated in Phase 1. Safe — ESP32 GPIOs default to input mode on reset. Will be configured properly in Phase 3.

---

## 6. Testing Coverage Audit

1. **No tests exist** for Epoch 1. The plan schedules unit tests for Epoch 3 (button mapper, PS1 protocol). Several Epoch 1 functions are pure logic and testable without hardware:
   - `ENC_TABLE` correctness (16-entry state machine validation)
   - `format_setting_value` (value formatting)
   - Menu state machine transitions
   - Encoder accumulator carry logic
   - `menu_get_item_label` bounds guard
2. **Testability prerequisite**: Adding tests would require `_reset_for_test()` functions or extracting pure logic into separate units. Deferred to Epoch 3 when the test infrastructure is set up.

---

## 7. DX & Maintainability Audit

1. **config.h** — `PIN_LED_1`, `PIN_LED_2`, `PIN_PS1_*`, `PS1_DEVICE_ID` etc. defined but unused. Accepted — these are reserved for Phase 2/3. Section headers already document this.
2. [FIXED] **display.cpp:65-67** — `static const` layout values moved to file scope with named constants (`SETTINGS_MAX_VISIBLE`, etc.).
3. [FIXED] **display.cpp:90** — Magic `3` replaced with `menu_get_editable_count()`.
4. [FIXED] **display.cpp:97,101** — Magic pixel offsets `90`/`95` replaced with named constants `SETTINGS_VAL_EDIT_COL`/`SETTINGS_VAL_COL`.
5. **display.h:15** — `display_get_screen()` has only one call site (now in main.cpp). Keeping it — useful API.
6. [FIXED] **menu.h:26** — "Epoch 3" terminology → changed to "Phase 3" for consistency.
7. [FIXED] **menu.cpp:92,95** — Magic menu indices `3`/`4` → `MENU_IDX_PAIRING`/`MENU_IDX_ABOUT`.
8. [FIXED] **menu.cpp:114,121** — Player number max hardcoded → `PLAYER_NUM_MIN`/`PLAYER_NUM_MAX`.
9. **menu.cpp:37-58** — Three parallel switch blocks for save/discard/snapshot. Accepted for 3 settings. Would refactor to table-driven if count grows past 5.
10. [FIXED] **input.cpp:80** — `(void*)0` etc. magic indices documented with comment in button_isr header.
11. [FIXED] **input.cpp:5** — `driver/gpio.h` include documented with WHY comment.
12. [FIXED] **main.cpp:44-49** — Unconditional GPIO reads guarded with screen check.
