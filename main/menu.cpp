#include "menu.h"
#include "bt.h"
#include "display.h"
#include "config.h"

#include <Arduino.h>
#include <Preferences.h>

// ── Menu Item Definitions ──────────────────────────────────────────

static const MenuItem MENU_ITEMS[] = {
    { MENU_HEADING, "Controller", nullptr,                        -1 },
    { MENU_VALUE,   "Trigger Thresh", "L2/R2 analog threshold",   0 },
    { MENU_VALUE,   "Stick to DPad",  "Map left stick to D-pad",  1 },
    { MENU_VALUE,   "Player Number",  "Console port: P1 or P2",   2 },
    { MENU_VALUE,   "Touchpad Sel/St","Touchpad left=Sel right=St",3 },
    { MENU_VALUE,   "Console Mode",   "Target console",            4 },
    { MENU_HEADING, "Device",         nullptr,                    -1 },
    { MENU_ACTION,  "Pairing",        "Pair DualSense controller",-1 },
    { MENU_ACTION,  "About",          "Firmware info",            -1 },
};
static const uint8_t MENU_ITEM_COUNT = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);

// Named setting IDs — match the setting_id field in MENU_ITEMS
static const int8_t SID_TRIGGER_THRESH  = 0;
static const int8_t SID_STICK_DPAD      = 1;
static const int8_t SID_PLAYER_NUM      = 2;
static const int8_t SID_TOUCHPAD_SELECT = 3;
static const int8_t SID_CONSOLE_MODE    = 4;

// Named action item indices — match positions in MENU_ITEMS[]
static const uint8_t IDX_PAIRING = 7;
static const uint8_t IDX_ABOUT   = 8;

// Console mode range
static const uint8_t CONSOLE_MODE_MIN = 0;  // PS1
static const uint8_t CONSOLE_MODE_MAX = 1;  // PS2

// Player number range
static const uint8_t PLAYER_NUM_MIN = 1;
static const uint8_t PLAYER_NUM_MAX = 2;

// ── State ──────────────────────────────────────────────────────────

static Preferences prefs;
static MenuState   state         = MENU_HOME;
static uint8_t     selected_item = 1;  // First selectable item
static uint8_t     scroll_offset = 0;

// Settings (loaded from NVS on init, written on confirm)
static uint8_t trigger_threshold;
static bool    stick_to_dpad;
static uint8_t player_number;
static bool    touchpad_select;
static uint8_t console_mode;

// Snapshot of setting value before edit (for discard-on-BAK)
static uint8_t edit_snapshot_u8;
static bool    edit_snapshot_bool;

// ── Helpers ────────────────────────────────────────────────────────

// Find next selectable item (skips headings). Returns `from` if nothing found.
static uint8_t find_next_selectable(uint8_t from, int8_t direction) {
    int16_t idx = (int16_t)from + direction;
    while (idx >= 0 && idx < MENU_ITEM_COUNT) {
        if (MENU_ITEMS[idx].type != MENU_HEADING) return (uint8_t)idx;
        idx += direction;
    }
    return from;  // No selectable item found — stay put
}

// Adjust scroll_offset so selected_item is visible in the viewport.
// When scrolling up to a group's first value item, include its preceding heading.
static void adjust_scroll() {
    // If a heading precedes the selected item, try to show it too
    if (selected_item > 0 && MENU_ITEMS[selected_item - 1].type == MENU_HEADING) {
        uint8_t heading_idx = selected_item - 1;
        if (heading_idx < scroll_offset) {
            scroll_offset = heading_idx;
        }
    }
    if (selected_item < scroll_offset) {
        scroll_offset = selected_item;
    }
    if (selected_item >= scroll_offset + MENU_VIEWPORT_ROWS) {
        scroll_offset = selected_item - MENU_VIEWPORT_ROWS + 1;
    }
}

static int8_t setting_id_of_selected() {
    if (selected_item < MENU_ITEM_COUNT) {
        return MENU_ITEMS[selected_item].setting_id;
    }
    return -1;
}

static void save_current_setting() {
    switch (setting_id_of_selected()) {
        case SID_TRIGGER_THRESH:  prefs.putUChar("trig_thresh", trigger_threshold); break;
        case SID_STICK_DPAD:      prefs.putBool("stick_dpad", stick_to_dpad);       break;
        case SID_PLAYER_NUM:      prefs.putUChar("player_num", player_number);      break;
        case SID_TOUCHPAD_SELECT: prefs.putBool("tp_select", touchpad_select);      break;
        case SID_CONSOLE_MODE:    prefs.putUChar("con_mode", console_mode);        break;
    }
}

static void discard_current_setting() {
    switch (setting_id_of_selected()) {
        case SID_TRIGGER_THRESH:  trigger_threshold = edit_snapshot_u8;   break;
        case SID_STICK_DPAD:      stick_to_dpad     = edit_snapshot_bool; break;
        case SID_PLAYER_NUM:      player_number     = edit_snapshot_u8;   break;
        case SID_TOUCHPAD_SELECT: touchpad_select   = edit_snapshot_bool; break;
        case SID_CONSOLE_MODE:    console_mode      = edit_snapshot_u8;   break;
    }
}

static void snapshot_current_setting() {
    switch (setting_id_of_selected()) {
        case SID_TRIGGER_THRESH:  edit_snapshot_u8   = trigger_threshold; break;
        case SID_STICK_DPAD:      edit_snapshot_bool = stick_to_dpad;     break;
        case SID_PLAYER_NUM:      edit_snapshot_u8   = player_number;     break;
        case SID_TOUCHPAD_SELECT: edit_snapshot_bool = touchpad_select;   break;
        case SID_CONSOLE_MODE:    edit_snapshot_u8   = console_mode;     break;
    }
}

static void format_setting_value(int8_t setting_id, char* buf, size_t len) {
    switch (setting_id) {
        case SID_TRIGGER_THRESH:  snprintf(buf, len, "%u", trigger_threshold); break;
        case SID_STICK_DPAD:      snprintf(buf, len, "%s", stick_to_dpad ? "ON" : "OFF"); break;
        case SID_PLAYER_NUM:      snprintf(buf, len, "P%u", player_number); break;
        case SID_TOUCHPAD_SELECT: snprintf(buf, len, "%s", touchpad_select ? "ON" : "OFF"); break;
        case SID_CONSOLE_MODE:    snprintf(buf, len, "%s", console_mode ? "PS2" : "PS1"); break;
        default: if (len > 0) buf[0] = '\0'; break;
    }
}

// ── Input Handling ─────────────────────────────────────────────────

static void handle_home(InputEvent evt) {
    if (evt == INPUT_BTN_CON || evt == INPUT_BTN_PHS) {
        input_flush_queue();
        state = MENU_SETTINGS;
        selected_item = find_next_selectable(0, 1);  // First selectable (idx 1)
        scroll_offset = 0;
        display_set_screen(SCREEN_MENU);
    }
}

static void handle_settings(InputEvent evt) {
    switch (evt) {
        case INPUT_ENC_CW: {
            uint8_t next = find_next_selectable(selected_item, 1);
            if (next != selected_item) {
                selected_item = next;
                adjust_scroll();
            }
            break;
        }
        case INPUT_ENC_CCW: {
            uint8_t prev = find_next_selectable(selected_item, -1);
            if (prev != selected_item) {
                selected_item = prev;
                adjust_scroll();
            }
            break;
        }
        case INPUT_BTN_CON:
        case INPUT_BTN_PHS:
            if (MENU_ITEMS[selected_item].type == MENU_VALUE) {
                snapshot_current_setting();
                state = MENU_SETTING_EDIT;
            } else if (MENU_ITEMS[selected_item].type == MENU_ACTION) {
                if (selected_item == IDX_PAIRING) {
                    state = MENU_PAIRING;
                    bt_start_pairing();
                    display_set_screen(SCREEN_PAIRING);
                } else if (selected_item == IDX_ABOUT) {
                    state = MENU_ABOUT;
                }
            }
            break;
        case INPUT_BTN_BAK:
            state = MENU_HOME;
            display_set_screen(SCREEN_VISUALIZER);
            break;
        default:
            break;
    }
}

static void handle_edit(InputEvent evt) {
    int8_t sid = setting_id_of_selected();
    switch (evt) {
        case INPUT_ENC_CW:
            switch (sid) {
                case SID_TRIGGER_THRESH:  if (trigger_threshold < 255) trigger_threshold++; break;
                case SID_STICK_DPAD:      stick_to_dpad = !stick_to_dpad; break;
                case SID_PLAYER_NUM:      player_number = (player_number >= PLAYER_NUM_MAX) ? PLAYER_NUM_MIN : (player_number + 1); break;
                case SID_TOUCHPAD_SELECT: touchpad_select = !touchpad_select; break;
                case SID_CONSOLE_MODE:   console_mode = (console_mode >= CONSOLE_MODE_MAX) ? CONSOLE_MODE_MIN : (console_mode + 1); break;
            }
            break;
        case INPUT_ENC_CCW:
            switch (sid) {
                case SID_TRIGGER_THRESH:  if (trigger_threshold > 0) trigger_threshold--; break;
                case SID_STICK_DPAD:      stick_to_dpad = !stick_to_dpad; break;
                case SID_PLAYER_NUM:      player_number = (player_number <= PLAYER_NUM_MIN) ? PLAYER_NUM_MAX : (player_number - 1); break;
                case SID_TOUCHPAD_SELECT: touchpad_select = !touchpad_select; break;
                case SID_CONSOLE_MODE:   console_mode = (console_mode <= CONSOLE_MODE_MIN) ? CONSOLE_MODE_MAX : (console_mode - 1); break;
            }
            break;
        case INPUT_BTN_CON:
        case INPUT_BTN_PHS:
            save_current_setting();
            state = MENU_SETTINGS;
            Serial.printf("[menu] saved setting_id=%d\n", sid);
            break;
        case INPUT_BTN_BAK:
            discard_current_setting();
            state = MENU_SETTINGS;
            break;
        default:
            break;
    }
}

static void handle_pairing(InputEvent evt) {
    if (evt == INPUT_BTN_BAK) {
        bt_stop_pairing();
        state = MENU_SETTINGS;
        display_set_screen(SCREEN_MENU);
    }
}

static void handle_about(InputEvent evt) {
    if (evt == INPUT_BTN_BAK) {
        state = MENU_SETTINGS;
    }
}

// ── Public API ─────────────────────────────────────────────────────

void menu_init() {
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        Serial.println("[menu] WARN: NVS open failed, using defaults");
    }
    trigger_threshold = prefs.getUChar("trig_thresh", TRIGGER_THRESHOLD_DEFAULT);
    stick_to_dpad     = prefs.getBool("stick_dpad",   STICK_TO_DPAD_DEFAULT);
    player_number     = prefs.getUChar("player_num",  PLAYER_NUM_MIN);
    touchpad_select   = prefs.getBool("tp_select",    TOUCHPAD_SELECT_DEFAULT);
    console_mode      = prefs.getUChar("con_mode",    CONSOLE_MODE_DEFAULT);

    // Validate NVS values (may be corrupt)
    if (player_number < PLAYER_NUM_MIN || player_number > PLAYER_NUM_MAX) {
        player_number = PLAYER_NUM_MIN;
    }
    if (console_mode < CONSOLE_MODE_MIN || console_mode > CONSOLE_MODE_MAX) {
        console_mode = CONSOLE_MODE_DEFAULT;
    }

    state = MENU_HOME;
    Serial.printf("[menu] loaded — thresh=%u dpad=%d player=P%u tp_sel=%d mode=%s\n",
                  trigger_threshold, stick_to_dpad, player_number, touchpad_select,
                  console_mode ? "PS2" : "PS1");
}

void menu_handle_input(InputEvent evt) {
    switch (state) {
        case MENU_HOME:         handle_home(evt);     break;
        case MENU_SETTINGS:     handle_settings(evt); break;
        case MENU_SETTING_EDIT: handle_edit(evt);     break;
        case MENU_PAIRING:      handle_pairing(evt);  break;
        case MENU_ABOUT:        handle_about(evt);    break;
    }
}

MenuState menu_get_state() {
    return state;
}

uint8_t menu_get_selected_item() {
    return selected_item;
}

uint8_t menu_get_item_count() {
    return MENU_ITEM_COUNT;
}

const MenuItem* menu_get_items() {
    return MENU_ITEMS;
}

uint8_t menu_get_scroll_offset() {
    return scroll_offset;
}

bool menu_is_editing() {
    return state == MENU_SETTING_EDIT;
}

void menu_get_value_str(int8_t setting_id, char* buf, size_t len) {
    format_setting_value(setting_id, buf, len);
}

bool menu_is_at_min() {
    switch (setting_id_of_selected()) {
        case SID_TRIGGER_THRESH:  return trigger_threshold == 0;
        case SID_STICK_DPAD:      return false;  // Bool always toggles
        case SID_PLAYER_NUM:      return player_number <= PLAYER_NUM_MIN;
        case SID_TOUCHPAD_SELECT: return false;  // Bool always toggles
        case SID_CONSOLE_MODE:    return console_mode <= CONSOLE_MODE_MIN;
        default: return false;
    }
}

bool menu_is_at_max() {
    switch (setting_id_of_selected()) {
        case SID_TRIGGER_THRESH:  return trigger_threshold == 255;
        case SID_STICK_DPAD:      return false;  // Bool always toggles
        case SID_PLAYER_NUM:      return player_number >= PLAYER_NUM_MAX;
        case SID_TOUCHPAD_SELECT: return false;  // Bool always toggles
        case SID_CONSOLE_MODE:    return console_mode >= CONSOLE_MODE_MAX;
        default: return false;
    }
}

uint8_t menu_get_trigger_threshold() {
    return trigger_threshold;
}

bool menu_get_stick_to_dpad() {
    return stick_to_dpad;
}

uint8_t menu_get_player_number() {
    return player_number;
}

bool menu_get_touchpad_select() {
    return touchpad_select;
}

uint8_t menu_get_console_mode() {
    return console_mode;
}
