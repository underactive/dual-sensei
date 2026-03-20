# Audit: Multi-Controller Support (DS4 + Xbox One + Switch Pro)

## Files changed

Findings flagged in the following files (including immediate dependents):
- `main/bt.h`
- `main/bt.cpp`
- `main/display.cpp`
- `main/menu.cpp`
- `docs/CLAUDE.md/testing-checklist.md`

---

## 1. QA Audit

| Severity | Finding |
|----------|---------|
| Warning | [FIXED] bt.cpp:39-40 — `controller_name[16]` could truncate long third-party controller names. All known Bluepad32 names fit (max 11 chars for "DualShock 4"), but edge case exists. Truncation is safe (snprintf bounded), cosmetic only. |
| Warning | bt.cpp:39 — `ctl->getModelName()` could return empty string for unrecognized controller. display.cpp:202 fallback `(name[0] != '\0') ? name : "Controller"` handles this correctly. |
| Info | display.cpp:201 — `bt_get_controller_name()` called directly from display renderer, bypassing the `display_set_controller()` data-flow pattern. Acceptable since both run on CPU1. |

## 2. Security Audit

| Severity | Finding |
|----------|---------|
| Warning | [FIXED] bt.cpp:40 — `model.c_str()` could return nullptr on OOM (Arduino String heap failure), causing UB when passed to `snprintf %s`. Added null guard: `cstr ? cstr : "Controller"`. |

## 3. Interface Contract Audit

| Severity | Finding |
|----------|---------|
| Warning | bt.cpp:156 / display.cpp:201 — `bt_get_controller_name()` returns pointer to mutable static buffer. Safe now (single-task on CPU1) but would become a data race if display rendering were moved to a separate FreeRTOS task (noted in Known Issues #3 as future work). |
| Warning | menu.cpp:250 — NVS open failure allows reads (returns defaults) but subsequent `save_current_setting()` writes silently fail. Pre-existing pattern, not introduced by this change. |
| Warning | bt.cpp:58 / display.cpp:21 — `vis_ctrl` in display.cpp could show one stale frame after disconnect, depending on main loop call ordering. Pre-existing pattern. |
| Warning | bt.cpp:13 — `controller_name[16]` truncation of long names. Duplicate of QA finding. |

## 4. State Management Audit

| Severity | Finding |
|----------|---------|
| Warning | bt.cpp:10-11 — `connected_gamepad` / `connected_mouse` pointers not declared `volatile` or `std::atomic`. Safe in practice (single-task CPU1, function call acts as compiler barrier) but formally fragile. Pre-existing pattern. |
| Info | display.cpp:201 — Cross-module direct read (`bt_get_controller_name()`) bypasses established push model. Functionally correct. |

## 5. Resource & Concurrency Audit

| Severity | Finding |
|----------|---------|
| Warning | bt.cpp:39 — Temporary Arduino `String` heap allocation in `on_connected()` callback. One-shot per connection event, not hot path. Acceptable per Development Rule #4. |
| Warning | menu.cpp:47 — NVS `Preferences` handle never closed. Acceptable for never-terminating embedded firmware. Pre-existing. |
| Warning | display.cpp:465 — `u8g2.sendBuffer()` blocks ~20ms per frame. Pre-existing, documented in Known Issues #3. |

## 6. Testing Coverage Audit

| Severity | Finding |
|----------|---------|
| Warning | [FIXED] Missing checklist item: touchpad virtual device lifecycle (connect/disconnect serial logs). Added. |
| Warning | [FIXED] Missing checklist item: rumble after disconnect/reconnect cycle. Added. |
| Warning | [FIXED] Missing checklist item: pairing screen visual transition when controller connects during pairing. Added. |
| Warning | NVS open failure degraded-mode behavior not testable manually. Deferred to Epoch 3 unit test infrastructure. |

## 7. DX & Maintainability Audit

| Severity | Finding |
|----------|---------|
| Warning | display.cpp:64-108 — Four face button draw functions share identical pressed/unpressed boilerplate. Pre-existing, not introduced by this change. |
| Warning | display.cpp:151-191 — PSX protocol bit positions use magic numbers instead of named constants. Pre-existing. |
| Warning | display.cpp:151-191 — `compute_protocol_bytes()` is protocol domain logic placed in display layer. Will need extraction for Phase 3 SPI. Pre-existing. |
| Warning | [FIXED] bt.h:18 — `bt_play_rumble` public API missing parameter range and no-op behavior documentation. Added inline doc comment. |

---

## Summary

| Audit | Critical | Warning | Info | Fixed |
|-------|----------|---------|------|-------|
| QA | 0 | 2 | 1 | 1 |
| Security | 0 | 1 | 0 | 1 |
| Interface Contract | 0 | 4 | 0 | 0 |
| State Management | 0 | 1 | 1 | 0 |
| Resource & Concurrency | 0 | 3 | 0 | 0 |
| Testing Coverage | 0 | 4 | 0 | 3 |
| DX & Maintainability | 0 | 4 | 0 | 1 |
| **Total** | **0** | **19** | **2** | **6** |

**0 critical issues.** Of 19 warnings, 6 were fixed immediately. The remaining 13 are either pre-existing patterns (NVS handle lifecycle, `connected_gamepad` volatility, I2C blocking, display boilerplate, protocol magic numbers) or acceptable trade-offs documented in the plan's Risks section (buffer truncation for unknown future controllers, single-task safety assumption for `controller_name` pointer).
