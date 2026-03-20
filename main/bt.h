#pragma once

#include "display.h"

void bt_init();
void bt_update();
bool bt_is_connected();
const ControllerState& bt_get_state();

// Controller identity (display-friendly name, e.g. "DualSense", "XBox One")
const char* bt_get_controller_name();

// Pairing control
void bt_start_pairing();
void bt_stop_pairing();

// Rumble output (sends haptic command to connected controller)
// weak/strong: motor intensity 0-255. No-op if no controller connected.
void bt_play_rumble(uint16_t duration_ms, uint8_t weak, uint8_t strong);
