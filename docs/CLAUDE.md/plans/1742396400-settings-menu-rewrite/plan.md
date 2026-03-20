# Plan: Rewrite Settings Menu (Ghost_operator Style)

## Objective

Replace the settings menu rendering and navigation with a ghost_operator-style layout: grouped items with headings, `< >` inline value editing with partial-row inversion, and a context-aware help bar at the bottom. Preserves all existing NVS persistence, settings values, and the external API contract with `main.cpp`.

## Layout (128x64 OLED, u8g2_font_5x7_tr)

```
Y=0..7    Header: "Settings" (8px)
Y=9       Separator line
Y=10..49  Viewport: 5 rows x 8px (scrollable)
Y=50      Separator line
Y=52..59  Help bar: context-aware text
```

### Three item types
1. **MENU_HEADING** — Centered `"- Label -"`, non-selectable, cursor skips
2. **MENU_VALUE** — Label left, value right. Selected: full row inverted. Editing: only value portion inverted with `< >` arrows
3. **MENU_ACTION** — Label left, `>` caret right. Selected: full row inverted

### Menu items (7 total)
| Idx | Type | Label | Help Text | setting_id |
|-----|------|-------|-----------|------------|
| 0 | HEADING | Controller | — | — |
| 1 | VALUE | Trigger Thresh | L2/R2 analog threshold | 0 |
| 2 | VALUE | Stick to DPad | Map left stick to D-pad | 1 |
| 3 | VALUE | Player Number | PS1 port: P1 or P2 | 2 |
| 4 | HEADING | Device | — | — |
| 5 | ACTION | Pairing | Pair DualSense controller | — |
| 6 | ACTION | About | Firmware info | — |

## Changes

### 1. `src/config.h`
Add menu layout constants: `MENU_VIEWPORT_Y`, `MENU_ROW_H`, `MENU_VIEWPORT_ROWS`, `MENU_HELP_BASELINE`.

### 2. `src/input.h` / `src/input.cpp`
Add `input_flush_queue()` — resets the FreeRTOS queue to prevent stale encoder events from carrying over into the settings screen.

### 3. `src/menu.h`
- Add `MenuItemType` enum, `MenuItem` struct
- Replace old getters (`menu_get_editable_count`, `menu_get_item_label`, `menu_get_edit_value`, `menu_get_edit_value_for`) with new API: `menu_get_items()`, `menu_get_scroll_offset()`, `menu_is_editing()`, `menu_get_value_str()`, `menu_is_at_min()`, `menu_is_at_max()`

### 4. `src/menu.cpp`
- Replace `MENU_LABELS[]` with `MENU_ITEMS[]` array of `MenuItem` structs
- Data-driven dispatch: switch on `setting_id` instead of raw indices
- `find_next_selectable()` helper to skip heading rows
- `adjust_scroll()` helper with heading-aware scrolling
- Queue flush on menu entry, PHS treated as CON in all menu states

### 5. `src/display.cpp`
- Replace `render_settings()` entirely with new viewport-based renderer
- Headings centered, values with selection inversion, actions with caret
- Partial-row inversion for edit mode with `< >` arrows
- Context-aware help bar at bottom
- Remove old layout constants

## Dependencies
- `menu.h` types must be defined before `display.cpp` and `menu.cpp` compile
- `input_flush_queue()` must exist before `menu.cpp` calls it

## Risks / Open Questions
- Font rendering in transparent mode (`setFontMode(1)`) is required for inverted text. Must restore `setFontMode(0)` at end to avoid affecting other screens.
- `< >` arrow visibility depends on `menu_is_at_min()`/`menu_is_at_max()` — boolean settings always show both arrows since they toggle.
