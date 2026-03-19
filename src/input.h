#pragma once

#include <cstdint>

enum InputEvent : uint8_t {
    INPUT_NONE = 0,
    INPUT_ENC_CW,       // Encoder rotated clockwise (one detent)
    INPUT_ENC_CCW,      // Encoder rotated counter-clockwise
    INPUT_BTN_CON,      // Confirm button pressed
    INPUT_BTN_BAK,      // Back button pressed
    INPUT_BTN_PHS,      // Encoder push-switch pressed
};

void       input_init();
InputEvent input_poll();
int32_t    input_get_encoder_pos();
