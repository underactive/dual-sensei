#pragma once

#include "display.h"

void bt_init();
void bt_update();
bool bt_is_connected();
const ControllerState& bt_get_state();

// Pairing control
void bt_start_pairing();
void bt_stop_pairing();

// Rumble output (sends haptic command to DualSense)
void bt_play_rumble(uint16_t duration_ms, uint8_t weak, uint8_t strong);
