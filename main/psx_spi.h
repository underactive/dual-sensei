#pragma once

#include "display.h"  // ControllerState
#include <cstdint>

// Initialize SPI3 (VSPI) as PSX controller slave.
// Configures GPIO matrix, SPI peripheral, and ISRs.
void psx_spi_init();

// Enable ATT interrupt — call when console is powered and ready.
void psx_spi_enable();

// Disable ATT interrupt — call to prevent ISR storms when console is off.
void psx_spi_disable();

// Cycle through SPI clock edge configurations (4 combinations).
void psx_spi_cycle_clock();

// Update the response buffer with fresh controller state.
// Called from the main loop after bt_update().
void psx_spi_set_state(const ControllerState& cs, uint8_t console_mode);

// Read and reset debug counters (returns ATT fall count and SPI byte count).
void psx_spi_read_counters(uint32_t* att_falls, uint32_t* spi_bytes);

// Read last transaction's CMD/DAT bytes for debugging.
void psx_spi_read_last_transaction(uint8_t* cmd_out, uint8_t* dat_out, uint8_t* len_out);

// Read SPI register state for diagnostics.
void psx_spi_read_diag(bool* clk_level, bool* spi_usr_armed, uint32_t* slave_reg);
