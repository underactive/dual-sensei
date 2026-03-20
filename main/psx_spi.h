#pragma once

#include "display.h"  // ControllerState
#include <cstdint>

// Initialize SPI3 (VSPI) as PSX controller slave.
// Configures GPIO matrix, SPI peripheral, and ISRs.
void psx_spi_init();

// Update the response buffer with fresh controller state.
// Called from the main loop after bt_update().
void psx_spi_set_state(const ControllerState& cs, uint8_t console_mode);
