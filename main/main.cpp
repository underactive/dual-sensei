#include <Arduino.h>
#include "config.h"
#include "bt.h"
#include "display.h"
#include "input.h"
#include "menu.h"

// ── Dual-Sensei Main ───────────────────────────────────────────────
// Phase 1: OLED display + encoder/button input + menu with NVS.
// Phase 2: Bluepad32 BT, DualSense input visualizer.
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
    bt_init();

    display_show_splash();
    delay(2000);

    // Transition to home visualizer screen and start render task
    display_set_screen(SCREEN_VISUALIZER);
    display_start_task();
    digitalWrite(PIN_LED_BT, LOW);

    Serial.println("[main] ready");
}

void loop() {
    // Poll input events from ISR queue
    InputEvent evt = input_poll();
    if (evt != INPUT_NONE) {
        menu_handle_input(evt);
    }

    // Poll Bluepad32 for controller data
    bt_update();

    // Push controller state to display task (visualizer reads it)
    if (display_get_screen() == SCREEN_VISUALIZER) {
        display_set_controller(bt_get_state());
    }

    // Serial commands: 's' = screenshot
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 's') display_screenshot();
    }

    // Yield to FreeRTOS scheduler — required with espidf framework
    // to avoid task watchdog timeout on the Arduino task.
    vTaskDelay(1);
}
