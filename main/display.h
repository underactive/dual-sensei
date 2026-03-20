#pragma once

#include <cstdint>

enum Screen : uint8_t {
    SCREEN_SPLASH,
    SCREEN_PAIRING,
    SCREEN_VISUALIZER,
    SCREEN_MENU,
};

// Controller state — PS1/PS2 buttons + PS2 analog sticks
struct ControllerState {
    bool connected = false;
    bool up = false, down = false, left = false, right = false;
    bool cross = false, circle = false, square = false, triangle = false;
    bool l1 = false, l2 = false, r1 = false, r2 = false;
    bool l3 = false, r3 = false;
    bool select = false, start = false;
    uint8_t lx = 0x80, ly = 0x80, rx = 0x80, ry = 0x80;  // Analog sticks (0-255, 128=center)
};

void   display_init();
void   display_show_splash();
void   display_start_task();  // Start FreeRTOS render task (call after splash)
void   display_set_screen(Screen screen);
Screen display_get_screen();

// Push controller state to the visualizer screen
void display_set_controller(const ControllerState& state);

// Request base64-encoded PNG screenshot (handled by display task)
void display_screenshot();
