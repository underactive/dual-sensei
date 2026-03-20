# Implementation: Rewrite Settings Menu (Ghost_operator Style)

## Files Changed

- `src/config.h` — Added `MENU_VIEWPORT_Y`, `MENU_ROW_H`, `MENU_VIEWPORT_ROWS`, `MENU_HELP_BASELINE` constants
- `src/input.h` — Added `input_flush_queue()` declaration
- `src/input.cpp` — Added `input_flush_queue()` implementation using `xQueueReset()`
- `src/menu.h` — Replaced old getter API with `MenuItemType`, `MenuItem`, and new getters (`menu_get_items`, `menu_get_scroll_offset`, `menu_is_editing`, `menu_get_value_str`, `menu_is_at_min`, `menu_is_at_max`)
- `src/menu.cpp` — Full rewrite: data-driven `MENU_ITEMS[]` array, `find_next_selectable()` for heading-skipping navigation, `adjust_scroll()` with heading awareness, `setting_id`-based dispatch for save/discard/snapshot/edit, queue flush on menu entry, PHS=CON in settings and edit states
- `src/display.cpp` — Replaced `render_settings()`: header+separator+viewport+separator+help bar layout, transparent font mode for inverted text, partial-row inversion for edit mode with `< >` arrows, context-aware help bar

## Summary

Implemented the ghost_operator-style settings menu as planned. Action item dispatch initially used `label[0] == 'P'`; replaced with named index constants after audit flagged it as fragile. Setting IDs replaced with named constants after DX audit.

## Verification

- `make build` — compiles cleanly with no new warnings (pre and post audit fixes)
- Manual verification pending: flash to ESP32 and test all menu interactions per the testing checklist

## Follow-ups

- Flash and verify on hardware (all items in testing checklist)
- Phase 3: When display moves to separate task, add mutex for `vis_ctrl` and menu state getters
- Phase 3: SPI mapper should read committed setting values, not in-progress edits
- Epoch 3: Unit tests for `find_next_selectable()`, `adjust_scroll()`, `handle_edit()` boundaries

## Audit Fixes

### Fixes Applied

1. **Action dispatch by named index** — Replaced fragile `label[0] == 'P'` dispatch with `IDX_PAIRING` / `IDX_ABOUT` named constants. (Interface Contract Audit §1, QA Audit §1, Security Audit §1, DX Audit §1)
2. **Named setting ID constants** — Replaced bare integers `0`, `1`, `2` with `SID_TRIGGER_THRESH`, `SID_STICK_DPAD`, `SID_PLAYER_NUM` across all 8 switch statements. (DX Audit §2)
3. **Computed separator position** — Replaced hardcoded `y=50` with `MENU_VIEWPORT_Y + MENU_VIEWPORT_ROWS * MENU_ROW_H`. (QA Audit §4)
4. **Player number default** — Changed bare `1` to `PLAYER_NUM_MIN` in `menu_init()` NVS load. (DX Audit §4)
5. **Testing checklist updated** — Added items for PHS-as-confirm, heading skip, viewport scrolling, arrow min/max, help bar context, queue flush. (Testing Coverage Audit §6-12)

### Verification Checklist

- [x] Build succeeds after all fixes (`make build` — SUCCESS)
- [ ] Verify named setting ID constants match MENU_ITEMS setting_id values (SID_TRIGGER_THRESH=0, SID_STICK_DPAD=1, SID_PLAYER_NUM=2)
- [ ] Verify IDX_PAIRING=5 and IDX_ABOUT=6 match their positions in MENU_ITEMS array
- [ ] Flash and verify Pairing/About actions still navigate correctly
- [ ] Verify separator line renders at correct position (y=50)

### Unresolved Items

- **Shared `edit_snapshot_u8`** (QA §2, Security §2) — Accepted for Phase 1. Only one setting edited at a time. Revisit if concurrent editing is introduced.
- **Screen/state routing asymmetry** (Interface §4-5, State §1) — About and Pairing use different routing patterns. Both work correctly. Consider deriving screen from MenuState in Phase 3 refactor.
- **`render_settings()` length** (DX §3) — 112 lines, above 50-line guideline. Internal structure is clear via switch/case. Accepted for now.
- **NVS write return values discarded** (Interface §5, Security §5) — No user-facing error display in Phase 1. Acceptable.
