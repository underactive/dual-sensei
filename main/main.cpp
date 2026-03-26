#include <Arduino.h>
#include "config.h"
#include "bt.h"
#include "display.h"
#include "input.h"
#include "menu.h"
#include "psx_spi.h"

// ── Dual-Sensei Main ───────────────────────────────────────────────
// Phase 1: OLED display + encoder/button input + menu with NVS.
// Phase 2: Bluepad32 BT + DualSense input visualizer + PSX SPI slave.

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
    psx_spi_init();

    display_show_splash();
    delay(2000);

    // Transition to home visualizer screen and start render task
    display_set_screen(SCREEN_VISUALIZER);
    display_start_task();
    digitalWrite(PIN_LED_BT, LOW);

    Serial.println("[main] ready");
}

// ── SPI Debug ────────────────────────────────────────────────────
static unsigned long last_diag_ms = 0;
static const unsigned long DIAG_INTERVAL_MS = 1000;

void loop() {
    // Poll input events from ISR queue
    InputEvent evt = input_poll();
    if (evt != INPUT_NONE) {
        menu_handle_input(evt);
    }

    // Poll Bluepad32 for controller data
    bt_update();

    // Push controller state to display task and SPI response buffer
    const ControllerState& cs = bt_get_state();
    if (display_get_screen() == SCREEN_VISUALIZER) {
        display_set_controller(cs);
    }
    psx_spi_set_state(cs, menu_get_console_mode());

    // SPI debug: print ATT/byte counters + register state every second
    unsigned long now = millis();
    if (now - last_diag_ms >= DIAG_INTERVAL_MS) {
        last_diag_ms = now;
        uint32_t att_falls = 0, spi_bytes = 0;
        psx_spi_read_counters(&att_falls, &spi_bytes);
        bool clk_lvl = false, usr_armed = false;
        uint32_t sreg = 0;
        psx_spi_read_diag(&clk_lvl, &usr_armed, &sreg);
        if (att_falls > 0 || spi_bytes > 0) {
            Serial.printf("[psx_spi] ATT:%lu  bytes:%lu  CLK:%d  USR:%d  SREG:0x%08lx\n",
                          att_falls, spi_bytes, clk_lvl, usr_armed, sreg);
            // Print last transaction CMD/DAT bytes
            uint8_t cmd_buf[10], dat_buf[10], tlen = 0;
            psx_spi_read_last_transaction(cmd_buf, dat_buf, &tlen);
            if (tlen > 0) {
                Serial.printf("  CMD:");
                for (uint8_t i = 0; i < tlen; i++) Serial.printf(" %02X", cmd_buf[i]);
                Serial.printf("\n  DAT:");
                for (uint8_t i = 0; i < tlen; i++) Serial.printf(" %02X", dat_buf[i]);
                Serial.printf("\n");
            }
            digitalWrite(PIN_LED_BT, !digitalRead(PIN_LED_BT));
        } else {
            digitalWrite(PIN_LED_BT, LOW);
        }
    }

    // Serial commands: 's' = screenshot, 'p' = enable PSX SPI, 'o' = disable PSX SPI
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 's') display_screenshot();
        else if (c == 'p') psx_spi_enable();
        else if (c == 'o') psx_spi_disable();
        else if (c == 'c') psx_spi_cycle_clock();
    }

    // Yield to FreeRTOS scheduler — required with espidf framework
    // to avoid task watchdog timeout on the Arduino task.
    vTaskDelay(1);
}
