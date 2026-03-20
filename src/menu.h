#pragma once

#include "input.h"
#include <cstddef>
#include <cstdint>

enum MenuState : uint8_t {
    MENU_HOME,          // Home screen (visualizer)
    MENU_SETTINGS,      // Settings list
    MENU_SETTING_EDIT,  // Editing a setting value (inline)
    MENU_PAIRING,       // Pairing instructions screen
    MENU_ABOUT,         // Firmware info screen
};

enum MenuItemType : uint8_t {
    MENU_HEADING,       // Non-selectable section heading
    MENU_VALUE,         // Editable setting (label + value)
    MENU_ACTION,        // Navigable action (label + > caret)
};

struct MenuItem {
    MenuItemType type;
    const char*  label;
    const char*  help;     // Help bar text (null for headings)
    int8_t       setting_id; // Index into settings array (-1 for non-value items)
};

void      menu_init();
void      menu_handle_input(InputEvent evt);
MenuState menu_get_state();

// State for display rendering
uint8_t          menu_get_selected_item();
uint8_t          menu_get_item_count();
const MenuItem*  menu_get_items();
uint8_t          menu_get_scroll_offset();
bool             menu_is_editing();
void             menu_get_value_str(int8_t setting_id, char* buf, size_t len);
bool             menu_is_at_min();
bool             menu_is_at_max();

// Settings accessors (used by button mapper in Phase 3)
uint8_t menu_get_trigger_threshold();
bool    menu_get_stick_to_dpad();
uint8_t menu_get_player_number();
