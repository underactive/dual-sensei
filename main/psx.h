#pragma once

#include "display.h"  // ControllerState
#include <cstdint>

// PSX response buffer size (max: ID + 0x5A + 6 data bytes for PS2 analog)
#define PSX_RESPONSE_MAX 8

// Build the PSX controller response for a poll (CMD 0x42).
// Fills resp[] with: [ID, 0x5A, buttons_lo, buttons_hi, (RX, RY, LX, LY for PS2)]
// Returns the number of bytes written (4 for PS1, 8 for PS2).
uint8_t psx_build_response(const ControllerState& cs, uint8_t console_mode,
                           uint8_t* resp);
