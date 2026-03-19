#pragma once

#include "input.h"
#include <cstddef>
#include <cstdint>

enum MenuState : uint8_t {
    MENU_HOME,          // Home screen (visualizer)
    MENU_SETTINGS,      // Settings list
    MENU_SETTING_EDIT,  // Editing a setting value
    MENU_PAIRING,       // Pairing instructions screen
    MENU_ABOUT,         // Firmware info screen
};

void      menu_init();
void      menu_handle_input(InputEvent evt);
MenuState menu_get_state();

// State for display rendering
uint8_t     menu_get_selected_item();
uint8_t     menu_get_item_count();
const char* menu_get_item_label(uint8_t idx);
void        menu_get_edit_value(char* buf, size_t len);
void        menu_get_edit_value_for(uint8_t idx, char* buf, size_t len);

// Settings accessors (used by button mapper in Epoch 3)
uint8_t menu_get_trigger_threshold();
bool    menu_get_stick_to_dpad();
uint8_t menu_get_player_number();
