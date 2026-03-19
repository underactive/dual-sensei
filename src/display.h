#pragma once

#include <cstdint>

enum Screen : uint8_t {
    SCREEN_SPLASH,
    SCREEN_PAIRING,
    SCREEN_VISUALIZER,
    SCREEN_MENU,
};

void   display_init();
void   display_show_splash();
void   display_set_screen(Screen screen);
Screen display_get_screen();
void   display_update();

// Data setters for the visualizer screen (Phase 1: encoder/button test)
void display_set_encoder_pos(int32_t pos);
void display_set_button_states(bool con, bool bak, bool phs);
