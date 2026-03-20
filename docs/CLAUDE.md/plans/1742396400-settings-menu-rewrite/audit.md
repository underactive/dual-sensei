# Audit Report: Settings Menu Rewrite

## Files Changed

Findings were flagged in the following files:
- `src/menu.cpp` — action dispatch, setting ID naming, snapshot sharing, NVS error handling
- `src/display.cpp` — render_settings length, hardcoded separator, heading buffer size
- `src/input.cpp` — input_flush_queue ISR safety note, input_poll null guard
- `src/config.h` — menu layout constants placement (minor)
- `src/main.cpp` — ControllerState copy-per-loop (informational, Epoch 2 concern)

---

## 1. QA Audit

1. **[FIXED] MEDIUM** `menu.cpp:148` — Action dispatch used fragile `label[0] == 'P'` instead of stable identifier. Fixed: now dispatches by named index constants `IDX_PAIRING` / `IDX_ABOUT`.

2. **MEDIUM** `menu.cpp:38-39` — `edit_snapshot_u8` shared between trigger_threshold and player_number. Safe because only one setting is edited at a time. Accepted as-is for Phase 1; revisit if concurrent editing is ever introduced.

3. **LOW** `menu.cpp/display.cpp` — Screen/state desync asymmetry: About stays on SCREEN_MENU, Pairing switches to SCREEN_PAIRING. Both work correctly but use different routing patterns. Accepted as-is — documented in CLAUDE.md Architecture section.

4. **[FIXED] LOW** `display.cpp:295` — Bottom separator hardcoded at y=50. Fixed: now computed as `MENU_VIEWPORT_Y + MENU_VIEWPORT_ROWS * MENU_ROW_H`.

5. **LOW** `display.cpp:227` — Heading buffer `char hdr[24]` is tight but safe for current labels. Accepted as-is.

6. **LOW** `display.cpp:244` — Edit decorated buffer `char decorated[16]` is safe for current values. Accepted as-is.

7. **LOW** `menu.cpp:117` — `find_next_selectable(0, 1)` called on every menu entry. Negligible cost with 7 items. Accepted as-is.

8. **LOW** `menu.cpp:29` — `selected_item` static initializer is 1, assuming idx 1 is selectable. Safe because `handle_home` overrides via `find_next_selectable()` on every entry. Accepted as-is.

9. **LOW** `menu.cpp:55-68` — Scroll heading-awareness is asymmetric (only on scroll-up). Intentional behavior per spec. Accepted as-is.

---

## 2. Security Audit

1. **[FIXED] MEDIUM** `menu.cpp:148` — Same as QA #1. Fixed via named index constants.

2. **MEDIUM** `menu.cpp:38-39` — Same as QA #2 (shared snapshot). Accepted as-is for Phase 1.

3. **LOW** `menu.cpp:266-268` — `menu_get_value_str` trusts caller setting_id. Default case handles unknown IDs safely. Accepted as-is.

4. **LOW** `display.cpp` — `item.label` not null-guarded. All static MENU_ITEMS have non-null labels. Accepted as-is.

5. **LOW** `menu.cpp:215-217` — NVS open failure continues silently; subsequent saves are no-ops. Acceptable for embedded device with no user-facing error display mechanism in Phase 1.

6. **LOW** `input.cpp:163-167` — `input_poll()` doesn't guard null queue (unlike flush). Safe because `input_init()` is always called before `input_poll()` in setup(). Accepted as-is.

---

## 3. Interface Contract Audit

1. **[FIXED] HIGH** `menu.cpp:148` — Same as QA #1. Fixed.

2. **MEDIUM** `menu.cpp:38-39` — Same as QA #2. Accepted as-is.

3. **MEDIUM** `menu.cpp:270-286` — `menu_is_at_min()`/`menu_is_at_max()` implicitly depend on selected_item. Safe in single-threaded loop. Accepted as-is.

4. **MEDIUM** `menu.cpp:146-164,206-210` — Pairing/About use asymmetric screen routing. Both work correctly. Accepted as-is.

5. **LOW** `menu.cpp:78-84` — `save_current_setting()` discards NVS write return value. Acceptable for Phase 1.

6. **LOW** `display.cpp:214-291` — Last viewport row's drawBox edge (y=50) coincides with separator at y=50. 1-pixel visual artifact when 5th row is selected. Accepted as cosmetic.

---

## 4. State Management Audit

1. **MEDIUM** `menu.cpp/display.cpp` — Dual ownership of screen state. Menu calls `display_set_screen()` imperatively. Works correctly but fragile. Noted for Phase 3 refactor when display moves to separate task.

2. **MEDIUM** `menu.cpp:38-39` — Same as QA #2. Accepted as-is.

3. **LOW** `menu.cpp:148` — Same as QA #1. **[FIXED]**.

4. **LOW** `display.cpp:376-378` — `vis_ctrl` copy unprotected. Safe in Phase 1. Needs mutex for Epoch 2 BT callbacks.

5. **LOW** `menu.cpp:167-197` — Settings mutated in-place during editing. Phase 3 SPI mapper should read committed values, not in-progress edits.

---

## 5. Resource & Concurrency Audit

1. **MEDIUM** `menu.cpp:27,215` — NVS Preferences handle never closed. Single-handle, single-namespace — acceptable for this application.

2. **MEDIUM** `display.cpp:397` — I2C bus contention when display task is separated. Future concern for Phase 3. No action needed now.

3. **LOW** `input.cpp:169-171` — `encoder_position` read without atomics. Safe on Xtensa LX6 (32-bit aligned atomic).

4. **LOW** `input.cpp:111-119` — Narrow race window during re-init. Re-init is uncommon. Accepted as-is.

5. **LOW** `input.cpp:173-176` — Queue flush not ISR-safe (correctly called from main loop only). Accepted as-is.

6. **LOW** `menu.cpp:28-35` — Menu state unprotected from concurrent access. Safe in single-threaded Arduino loop. Needs sync for Phase 3.

---

## 6. Testing Coverage Audit

1. **MEDIUM** — No unit tests for `find_next_selectable()`, `adjust_scroll()`, `handle_edit()` boundary logic. Deferred to Epoch 3 per project plan.

2. **[FIXED] HIGH** — Testing checklist updated with PHS-as-confirm, heading skip behavior, scroll/viewport items, arrow min/max indicators, help bar context changes, queue flush item.

3. **LOW** — No unit tests for `compute_ps1_bytes()`, `format_setting_value()`. Deferred to Epoch 3.

---

## 7. DX & Maintainability Audit

1. **[FIXED] MEDIUM** `menu.cpp:148` — Same as QA #1. Fixed.

2. **[FIXED] MEDIUM** `menu.cpp` — Setting IDs were bare integers across 8 switch statements. Fixed: now uses `SID_TRIGGER_THRESH`, `SID_STICK_DPAD`, `SID_PLAYER_NUM` named constants.

3. **MEDIUM** `display.cpp:198-310` — `render_settings()` is 112 lines. Could be split into helper functions per item type. Accepted as-is for Phase 1 — the function has clear internal structure with switch/case branches.

4. **[FIXED] LOW** `menu.cpp:220` — Player number default was bare `1`. Fixed: now uses `PLAYER_NUM_MIN`.

5. **LOW** `config.h:44-47` — Menu layout constants in project-wide config. Accepted as-is — follows existing project convention of all constants in config.h.

6. **LOW** — Minor code duplication in render_settings (normal vs selected value rendering). Accepted as-is.
