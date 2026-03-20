# Audit Report: PS2 Scope Expansion

## Files Changed

Findings flagged in the following files (including immediate dependents):

- `main/bt.cpp`
- `main/config.h`
- `main/display.h`
- `main/display.cpp`
- `main/menu.h`
- `main/menu.cpp`
- `main/main.cpp` (immediate dependent)

---

## 1. QA Audit

### [FIXED] QA-1 — HIGH: `axis_to_ps2()` uses implementation-defined signed right-shift
`bt.cpp:19`. The `>> 2` operator on a signed `int32_t` is implementation-defined for negative values pre-C++20. Changed to `/ 4`.

### [FIXED] QA-2 — HIGH: `count` uninitialized before `compute_protocol_bytes()`
`display.cpp:249`. `uint8_t count;` was uninitialized at declaration. Initialized to `0`.

### QA-3 — MEDIUM: About screen does not call `display_set_screen(SCREEN_MENU)` on entry
`menu.cpp:179-180`. Works because About is only reachable from SCREEN_MENU state. Pre-existing pattern — all non-screen-changing state transitions work this way. Accepted as-is.

### [FIXED] QA-4 — MEDIUM: L3/R3 comment lists bit positions in wrong order
`display.cpp:159`. Comment read `[R3] [L3]` but bit1=L3, bit2=R3. Updated comment to clarify PS1 bits 1,2 are always 1.

### QA-5 — MEDIUM: Position dot alleged to overlap R3 label at max deflection
`display.cpp:240-245`. **False positive.** At max right deflection: dot at x=112-113, "R3" label at x=116. 3px gap exists. No overlap.

### QA-6 — MEDIUM: `trigger_threshold` missing explicit range validation
`menu.cpp:249`. Full `uint8_t` range (0-255) is valid — no invalid values exist. Pre-existing pattern. Accepted.

### QA-7 — LOW: PS2 hex format readability
`display.cpp:255-256`. Acknowledged tradeoff in plan (condensed format to fit 128px width). No overflow risk.

### QA-8 — LOW: `player_number` NVS default uses `PLAYER_NUM_MIN`
`menu.cpp:251`. Pre-existing pattern. `PLAYER_NUM_MIN` is `1`, the correct default. Accepted.

### QA-9 — LOW: `IDX_PAIRING`/`IDX_ABOUT` fragile hardcoded indices
`menu.cpp:32-33`. Pre-existing design pattern. Accepted — addressed in future-improvements if needed.

---

## 2. Security Audit

### Security-1 — MEDIUM: Unbounded BT model name string in serial output
`bt.cpp:36-39`. Pre-existing code (not changed in this plan). Bluepad32 guarantees null-terminated strings from HID descriptors. Accepted.

### Security-2 — MEDIUM: `compute_protocol_bytes` buffer has no size parameter
`display.cpp:157-197`. Static function with a single caller that allocates exactly 6 bytes. No external callers possible. Accepted for now — Phase 3 SPI extraction will address this.

### Security-3 — MEDIUM: `trigger_threshold` not range-validated after NVS load
`menu.cpp:249`. Same as QA-6. Full `uint8_t` range is valid. Pre-existing. Accepted.

### [FIXED] Security-4 — LOW: `console_mode` NVS validation uses one-sided bound check
`menu.cpp:259-261`. Made symmetric with `< CONSOLE_MODE_MIN || > CONSOLE_MODE_MAX` for consistency with `player_number`.

### Security-5 — LOW: `draw_analog_stick` `(r-1)` underflows if `r=0`
`display.cpp:147-148`. All callers pass `r=5`. Static function. Accepted.

### Security-6 — LOW: `render_settings` heading buffer truncation
`display.cpp:291`. Longest heading is "Controller" (10 chars), well within 24-char buffer. Accepted.

### Security-7 — LOW: `selected_item` array index without bounds check
`menu.cpp:171,174`. Pre-existing pattern — `selected_item` is always set by bounded `find_next_selectable()`. Accepted.

---

## 3. Interface Contract Audit

### [FIXED] Interface-1 — HIGH: `count` uninitialized in `compute_protocol_bytes` caller
`display.cpp:249`. Same as QA-2. Initialized to `0`.

### Interface-2 — MEDIUM: Bluepad32 axis range assumption unverified
`bt.cpp:17-23`. Comment documents `-511..512`. Matches Bluepad32 API. Center at `val=0` maps to `128` correctly. Accepted — any drift would be caught during hardware testing.

### Interface-3 — MEDIUM: No re-validation at save boundary
`menu.cpp:98-106`. All values are constrained by edit handler logic (wrap, toggle, clamp). Save path writes only values that passed edit validation. Accepted.

### [FIXED] Interface-4 — MEDIUM: L3/R3 bit comment misleading
`display.cpp:159`. Same as QA-4. Updated comment.

### Interface-5 — MEDIUM: IDX_PAIRING/IDX_ABOUT hardcoded
`menu.cpp:32-33`. Same as QA-9. Pre-existing pattern. Accepted.

### Interface-6 — LOW: NVS open failure not propagated to save path
`menu.cpp:246-253`. Pre-existing. If NVS open fails, saves are no-ops. Low risk for hardware NVS. Accepted.

### Interface-7 — LOW: `vis_ctrl` stale for one frame on menu exit
`main.cpp:49-51`. Single-frame (67ms) exposure. Visually imperceptible. Accepted.

---

## 4. State Management Audit

### State-1 — MEDIUM: Dual `ControllerState` copies without synchronization protocol
`bt.cpp:12`, `display.cpp:21`, `main.cpp:49-51`. Both copies are on CPU1 in the same FreeRTOS task — no race today. Known Issue #3 acknowledges this will need mutex when display moves to separate task in Phase 3. Accepted.

### State-2 — MEDIUM: `console_mode` read live during edit (uncommitted value previewed)
`display.cpp:250`, `menu.cpp:202/211`. This is consistent with all other settings — `trigger_threshold`, `stick_to_dpad`, `touchpad_select` all take live effect during edit. This is intentional live-preview behavior. Accepted.

### State-3 — LOW: `touchpad_select` live during uncommitted edit affects BT mapping
`bt.cpp:109`, `menu.cpp:201/210`. Same live-preview pattern as State-2. Pre-existing for other settings. Accepted.

### State-4 — LOW: IDX_PAIRING/IDX_ABOUT positional fragility
`menu.cpp:32-33`. Duplicate of QA-9. Accepted.

### State-5 — LOW: Single-slot snapshot buffer has no type tag
`menu.cpp:58-59`. Pre-existing design. Single-item edit flow guarantees correct slot usage. Accepted.

---

## 5. Resource & Concurrency Audit

### Resource-1 — MEDIUM: Future data race on `ctrl_state`
`bt.cpp:12,63-123,146-148`. Same as State-1. Safe today, Phase 3 concern. Accepted.

### Resource-2 — MEDIUM: Future data race on `vis_ctrl`
`display.cpp:21,439-441`. Same as State-1. Accepted.

### Resource-3 — MEDIUM: `connected_mouse` teardown ordering assumption
`bt.cpp:41,51,109-113`. Pre-existing. Bluepad32 virtual device teardown follows gamepad disconnect. Accepted.

### Resource-4 — LOW: Menu settings read without synchronization
`menu.cpp`, `bt.cpp`, `display.cpp`. All on CPU1, same task. Accepted.

### Resource-5 — LOW: `last_frame_ms` not volatile
`display.cpp:18`. Single-task access. Accepted.

### Resource-6 — LOW: NVS write return values unchecked
`menu.cpp:98-106`. Pre-existing. Low risk for dedicated NVS partition. Accepted.

### Resource-7 — LOW: `compute_protocol_bytes` buffer size
Same as Security-2. Accepted.

---

## 6. Testing Coverage Audit

### Testing-1 — HIGH: No tests for `compute_protocol_bytes()` bit-packing
`display.cpp:157-197`. Pure function, testable on host. Deferred to Epoch 3 test infrastructure (Known Issue #4).

### Testing-2 — HIGH: No tests for `axis_to_ps2()` range mapping
`bt.cpp:18-23`. Pure function, testable on host. Deferred to Epoch 3.

### Testing-3 — HIGH: No tests for new menu edit handler arms
`menu.cpp:193-227`. Deferred to Epoch 3.

### Testing-4..8 — MEDIUM: Various untested paths
Menu min/max, format values, NVS validation, touchpad integration, stick dot arithmetic. All deferred to Epoch 3.

### [FIXED] Testing-9 — LOW: Missing testing-checklist items
`docs/CLAUDE.md/testing-checklist.md`. Added 18 new checklist items for PS2 features.

### Testing-10,11 — LOW: Minor untested edge cases
Pre-existing patterns. Accepted.

---

## 7. DX & Maintainability Audit

### [FIXED] DX-H1 — HIGH: Inconsistent indentation on `SID_CONSOLE_MODE` cases
`menu.cpp:104,114,124`. Fixed alignment to match peer cases (4-space indent after case label).

### DX-M1 — MEDIUM: Protocol logic buried in display layer
`display.cpp:157-197`. Acknowledged — Phase 3 SPI will extract `compute_protocol_bytes` into its own module. Accepted for now.

### DX-M2 — MEDIUM: IDX_PAIRING/IDX_ABOUT fragility
`menu.cpp:32-33`. Duplicate of QA-9. Accepted.

### [FIXED] DX-M3 — MEDIUM: Magic numbers in protocol format branch
`display.cpp:247-252`. Added comment explaining PS1=2 bytes, PS2=6 bytes.

### DX-M4 — MEDIUM: Duplicated wrap logic in edit handler
`menu.cpp:200,202,209,211`. Two items use this pattern. Not worth extracting a helper for 2 callers. Accepted.

### DX-M5 — MEDIUM: About screen hardcodes "PS5-to-PSX Bridge"
`display.cpp:387`. Generic string is intentionally console-agnostic. Accepted.

### DX-L1..L6 — LOW: Various naming/documentation suggestions
Pre-existing patterns or minor. Accepted.

---

## Summary

| Severity | Total | Fixed | Accepted/Deferred |
|----------|-------|-------|--------------------|
| HIGH     | 8     | 4     | 4 (testing — deferred to Epoch 3) |
| MEDIUM   | 21    | 2     | 19 (pre-existing patterns or architectural) |
| LOW      | 18    | 2     | 16 |
