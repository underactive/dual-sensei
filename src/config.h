#pragma once

// ── Firmware Identity ──────────────────────────────────────────────
#define FW_NAME    "DUAL-SENSEI"
#define FW_VERSION "0.1.0"

// ── I2C — OLED Display ────────────────────────────────────────────
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22

// ── Rotary Encoder ─────────────────────────────────────────────────
#define PIN_ENC_A   32  // TRA (quadrature channel A)
#define PIN_ENC_B   33  // TRB (quadrature channel B)
#define PIN_ENC_SW  25  // PHS (encoder push switch)

// ── Navigation Buttons ─────────────────────────────────────────────
#define PIN_BTN_CON 26  // Confirm
#define PIN_BTN_BAK 27  // Back

// ── Debug LEDs (Phase 1 only) ──────────────────────────────────────
#define PIN_LED_BT  2   // Onboard LED — BT status
#define PIN_LED_1   16  // Test LED 1
#define PIN_LED_2   17  // Test LED 2

// ── PS1 SPI — VSPI (Phase 2) ──────────────────────────────────────
#define PIN_PS1_CLK 18  // SPI CLK  — from PS1 pin 7
#define PIN_PS1_CMD 23  // SPI MOSI — from PS1 pin 2
#define PIN_PS1_DAT 19  // SPI MISO — to PS1 pin 1 (open-drain)
#define PIN_PS1_ATT 5   // SPI CS   — from PS1 pin 6
#define PIN_PS1_ACK 4   // ACK pulse — to PS1 pin 9 (open-drain)

// ── Display Constants ──────────────────────────────────────────────
#define OLED_WIDTH       128
#define OLED_HEIGHT      64
#define DISPLAY_FPS      15
#define DISPLAY_FRAME_MS (1000 / DISPLAY_FPS)

// ── Input Constants ────────────────────────────────────────────────
#define DEBOUNCE_US           50000  // 50ms button debounce (microseconds)
#define INPUT_QUEUE_SIZE      8
#define ENC_STEPS_PER_DETENT  4      // Quadrature transitions per encoder click

// ── PS1 Protocol Constants ─────────────────────────────────────────
#define PS1_DEVICE_ID    0x41   // Digital pad controller ID
#define PS1_DATA_MARKER  0x5A   // "Data ready" byte
#define PS1_IDLE_BYTE    0xFF   // All buttons released (active-low)

// ── Button Mapping Defaults ────────────────────────────────────────
#define TRIGGER_THRESHOLD_DEFAULT  128    // L2/R2 analog-to-digital threshold (0-255)
#define STICK_TO_DPAD_DEFAULT      false  // Left stick → D-pad mapping

// ── NVS Namespace ──────────────────────────────────────────────────
#define NVS_NAMESPACE "dual-sensei"
