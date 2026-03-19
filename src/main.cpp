#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "input.h"
#include "menu.h"

// ── Dual-Sensei Main ───────────────────────────────────────────────
// Phase 1: OLED display + encoder/button input + menu with NVS.
// Phase 2 adds: Bluepad32 BT, DualSense input visualizer.
// Phase 3 adds: PS5→PS1 button mapping, SPI slave protocol.

void setup() {
    Serial.begin(115200);
    delay(100);  // Allow serial to stabilize
    Serial.println();
    Serial.println("=== DUAL-SENSEI v" FW_VERSION " ===");

    // Debug LED — on during init
    pinMode(PIN_LED_BT, OUTPUT);
    digitalWrite(PIN_LED_BT, HIGH);

    display_init();
    input_init();
    menu_init();

    display_show_splash();
    delay(2000);

    // Transition to home visualizer screen
    display_set_screen(SCREEN_VISUALIZER);
    digitalWrite(PIN_LED_BT, LOW);

    Serial.println("[main] ready");
}

void loop() {
    // Poll input events from ISR queue
    InputEvent evt = input_poll();
    if (evt != INPUT_NONE) {
        menu_handle_input(evt);
    }

    // Push controller state to display only when visualizer is active.
    // Phase 1: empty state (no BT yet). Epoch 2 populates from DualSense.
    if (display_get_screen() == SCREEN_VISUALIZER) {
        ControllerState ctrl;  // Default: all released, not connected
        display_set_controller(ctrl);
    }

    // Render (throttled internally to ~15 FPS)
    display_update();
}
