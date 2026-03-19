#pragma once

#include <cstdint>

enum Screen : uint8_t {
    SCREEN_SPLASH,
    SCREEN_PAIRING,
    SCREEN_VISUALIZER,
    SCREEN_MENU,
};

// PS1 controller button state — populated by BT module in Epoch 2
struct ControllerState {
    bool connected = false;
    bool up = false, down = false, left = false, right = false;
    bool cross = false, circle = false, square = false, triangle = false;
    bool l1 = false, l2 = false, r1 = false, r2 = false;
    bool select = false, start = false;
};

void   display_init();
void   display_show_splash();
void   display_set_screen(Screen screen);
Screen display_get_screen();
void   display_update();

// Push controller state to the visualizer screen
void display_set_controller(const ControllerState& state);
