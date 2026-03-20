#pragma once

#include "display.h"

void bt_init();
void bt_update();
bool bt_is_connected();
const ControllerState& bt_get_state();

// Pairing control
void bt_start_pairing();
void bt_stop_pairing();
