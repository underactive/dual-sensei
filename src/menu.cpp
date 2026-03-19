#include "menu.h"
#include "display.h"
#include "config.h"

#include <Arduino.h>
#include <Preferences.h>

// ── Menu Item Definitions ──────────────────────────────────────────

static const char* MENU_LABELS[] = {
    "Trigger Thresh",
    "Stick -> DPad",
    "Player Number",
    "Pairing...",
    "About...",
};
static const uint8_t MENU_ITEM_COUNT = sizeof(MENU_LABELS) / sizeof(MENU_LABELS[0]);
static const uint8_t EDITABLE_COUNT  = 3;  // First 3 items are editable settings

// Named indices for non-editable action items
static const uint8_t MENU_IDX_PAIRING = 3;
static const uint8_t MENU_IDX_ABOUT   = 4;

// Player number range
static const uint8_t PLAYER_NUM_MIN = 1;
static const uint8_t PLAYER_NUM_MAX = 2;

// ── State ──────────────────────────────────────────────────────────

static Preferences prefs;
static MenuState   state         = MENU_HOME;
static uint8_t     selected_item = 0;

// Settings (loaded from NVS on init, written on confirm)
static uint8_t trigger_threshold;
static bool    stick_to_dpad;
static uint8_t player_number;

// Snapshot of setting value before edit (for discard-on-BAK)
static uint8_t edit_snapshot_u8;
static bool    edit_snapshot_bool;

// ── Helpers ────────────────────────────────────────────────────────

static void save_current_setting() {
    switch (selected_item) {
        case 0: prefs.putUChar("trig_thresh", trigger_threshold); break;
        case 1: prefs.putBool("stick_dpad", stick_to_dpad);       break;
        case 2: prefs.putUChar("player_num", player_number);      break;
    }
}

static void discard_current_setting() {
    switch (selected_item) {
        case 0: trigger_threshold = edit_snapshot_u8;   break;
        case 1: stick_to_dpad     = edit_snapshot_bool; break;
        case 2: player_number     = (uint8_t)edit_snapshot_u8; break;
    }
}

static void snapshot_current_setting() {
    switch (selected_item) {
        case 0: edit_snapshot_u8   = trigger_threshold; break;
        case 1: edit_snapshot_bool = stick_to_dpad;     break;
        case 2: edit_snapshot_u8   = player_number;     break;
    }
}

static void format_setting_value(uint8_t idx, char* buf, size_t len) {
    switch (idx) {
        case 0: snprintf(buf, len, "%u", trigger_threshold); break;
        case 1: snprintf(buf, len, "%s", stick_to_dpad ? "ON" : "OFF"); break;
        case 2: snprintf(buf, len, "P%u", player_number); break;
        default: buf[0] = '\0'; break;
    }
}

// ── Input Handling ─────────────────────────────────────────────────

static void handle_home(InputEvent evt) {
    if (evt == INPUT_BTN_CON || evt == INPUT_BTN_PHS) {
        state = MENU_SETTINGS;
        selected_item = 0;
        display_set_screen(SCREEN_MENU);
    }
}

static void handle_settings(InputEvent evt) {
    switch (evt) {
        case INPUT_ENC_CW:
            if (selected_item < MENU_ITEM_COUNT - 1) selected_item++;
            break;
        case INPUT_ENC_CCW:
            if (selected_item > 0) selected_item--;
            break;
        case INPUT_BTN_CON:
            if (selected_item < EDITABLE_COUNT) {
                snapshot_current_setting();
                state = MENU_SETTING_EDIT;
            } else if (selected_item == MENU_IDX_PAIRING) {
                state = MENU_PAIRING;
                display_set_screen(SCREEN_PAIRING);
            } else if (selected_item == MENU_IDX_ABOUT) {
                state = MENU_ABOUT;
                // Screen stays SCREEN_MENU; render_menu dispatches to render_about
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
    switch (evt) {
        case INPUT_ENC_CW:
            switch (selected_item) {
                case 0: if (trigger_threshold < 255) trigger_threshold++; break;
                case 1: stick_to_dpad = !stick_to_dpad; break;
                case 2: player_number = (player_number >= PLAYER_NUM_MAX) ? PLAYER_NUM_MIN : (player_number + 1); break;
            }
            break;
        case INPUT_ENC_CCW:
            switch (selected_item) {
                case 0: if (trigger_threshold > 0) trigger_threshold--; break;
                case 1: stick_to_dpad = !stick_to_dpad; break;
                case 2: player_number = (player_number <= PLAYER_NUM_MIN) ? PLAYER_NUM_MAX : (player_number - 1); break;
            }
            break;
        case INPUT_BTN_CON:
            save_current_setting();
            state = MENU_SETTINGS;
            Serial.printf("[menu] saved setting %u\n", selected_item);
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
        state = MENU_SETTINGS;
        display_set_screen(SCREEN_MENU);
    }
}

static void handle_about(InputEvent evt) {
    if (evt == INPUT_BTN_BAK) {
        state = MENU_SETTINGS;
        // Screen is already SCREEN_MENU (set when entering settings)
    }
}

// ── Public API ─────────────────────────────────────────────────────

void menu_init() {
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        Serial.println("[menu] WARN: NVS open failed, using defaults");
    }
    trigger_threshold = prefs.getUChar("trig_thresh", TRIGGER_THRESHOLD_DEFAULT);
    stick_to_dpad     = prefs.getBool("stick_dpad",   STICK_TO_DPAD_DEFAULT);
    player_number     = prefs.getUChar("player_num",  1);

    // Validate NVS values (may be corrupt)
    if (player_number < PLAYER_NUM_MIN || player_number > PLAYER_NUM_MAX) {
        player_number = PLAYER_NUM_MIN;
    }

    state = MENU_HOME;
    Serial.printf("[menu] loaded — thresh=%u dpad=%d player=P%u\n",
                  trigger_threshold, stick_to_dpad, player_number);
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

uint8_t menu_get_editable_count() {
    return EDITABLE_COUNT;
}

const char* menu_get_item_label(uint8_t idx) {
    if (idx >= MENU_ITEM_COUNT) return "";
    return MENU_LABELS[idx];
}

void menu_get_edit_value(char* buf, size_t len) {
    format_setting_value(selected_item, buf, len);
}

void menu_get_edit_value_for(uint8_t idx, char* buf, size_t len) {
    format_setting_value(idx, buf, len);
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
