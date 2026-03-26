#include "psx_spi.h"
#include "psx.h"
#include "config.h"

#include <Arduino.h>
#include "driver/gpio.h"
#include "soc/gpio_struct.h"
#include "rom/ets_sys.h"

// ── GPIO Bit-Bang PSX Slave ─────────────────────────────────────
// Instead of using the ESP32 SPI peripheral (which has numerous
// register-level quirks with slave mode), we bit-bang the PSX protocol
// directly using GPIO reads/writes inside the ATT ISR. This is the
// same approach BlueRetro uses and is more reliable.
//
// PSX SPI Mode 3 (CPOL=1, CPHA=1):
//   CLK idles HIGH
//   Data changes on CLK falling edge
//   Data sampled on CLK rising edge
//   LSB first

// ── GPIO masks for direct register access (IRAM-safe) ───────────
#define CLK_MASK  (1 << PIN_PSX_CLK)
#define CMD_MASK  (1 << PIN_PSX_CMD)
#define DAT_MASK  (1 << PIN_PSX_DAT)
#define ATT_MASK  (1 << PIN_PSX_ATT)
#define ACK_MASK  (1 << PIN_PSX_ACK)

// ── Transaction State ───────────────────────────────────────────

// Response buffer: [0xFF, ID, 0x5A, buttons_lo, buttons_hi, (RX, RY, LX, LY)]
static uint8_t DRAM_ATTR response_buf[PSX_RESPONSE_MAX + 1];
static volatile uint8_t DRAM_ATTR response_len = 5;

// ── Debug Counters (ISR-written, main-loop-read) ────────────────
static volatile uint32_t DRAM_ATTR att_fall_count = 0;
static volatile uint32_t DRAM_ATTR spi_byte_count = 0;

// ── Debug: CMD byte capture (last transaction) ──────────────────
#define PSX_DBG_MAX 10
static uint8_t DRAM_ATTR dbg_cmd[PSX_DBG_MAX];
static uint8_t DRAM_ATTR dbg_dat[PSX_DBG_MAX];
static volatile uint8_t DRAM_ATTR dbg_len = 0;

// Pending response (written by main loop, copied to response_buf on ATT fall)
static uint8_t pending_buf[PSX_RESPONSE_MAX + 1];
static volatile uint8_t pending_len = 5;
static portMUX_TYPE response_mux = portMUX_INITIALIZER_UNLOCKED;

// ── ACK Pulse ────────────────────────────────────────────────────

static inline void IRAM_ATTR send_ack() {
    ets_delay_us(PSX_ACK_DELAY_US);
    GPIO.out_w1tc = ACK_MASK;   // Drive LOW
    ets_delay_us(PSX_ACK_PULSE_US);
    GPIO.out_w1ts = ACK_MASK;   // Release HIGH
}

// ── ATT ISR — handles entire PSX transaction via bit-banging ────
//
// When ATT falls, the console is starting a transaction. We handle
// the full byte exchange synchronously: for each byte, wait for 8
// CLK edges, shift bits in/out on GPIO, then send ACK.
//
// At 250kHz PSX clock, each byte takes ~32μs. A 5-byte PS1 transaction
// takes ~160μs + ACK overhead ≈ 220μs total. Well within WDT limits.

static void IRAM_ATTR att_isr(void* arg) {
    // Only handle falling edge (transaction start)
    if (GPIO.in & ATT_MASK) return;

    att_fall_count = att_fall_count + 1;

    // Snapshot pending response data
    portENTER_CRITICAL_ISR(&response_mux);
    memcpy(response_buf, pending_buf, pending_len);
    response_len = pending_len;
    portEXIT_CRITICAL_ISR(&response_mux);

    uint8_t len = response_len;
    uint8_t bytes_done = 0;

    for (uint8_t byte_idx = 0; byte_idx < len; byte_idx++) {
        uint8_t tx = response_buf[byte_idx];
        uint8_t rx = 0;

        for (uint8_t bit = 0; bit < 8; bit++) {
            // Wait for CLK falling edge (CLK goes LOW)
            while (GPIO.in & CLK_MASK) {
                if (GPIO.in & ATT_MASK) goto done;  // ATT rose = abort
            }

            // Output bit on DAT (data changes on falling edge, LSB first)
            if (tx & (1 << bit))
                GPIO.out_w1ts = DAT_MASK;  // Release HIGH (open-drain float)
            else
                GPIO.out_w1tc = DAT_MASK;  // Drive LOW

            // Wait for CLK rising edge (CLK goes HIGH)
            while (!(GPIO.in & CLK_MASK)) {
                if (GPIO.in & ATT_MASK) goto done;  // ATT rose = abort
            }

            // Sample CMD on rising edge (LSB first)
            if (GPIO.in & CMD_MASK) rx |= (1 << bit);
        }

        bytes_done = bytes_done + 1;

        // Release DAT after each byte (return to idle HIGH)
        GPIO.out_w1ts = DAT_MASK;

        // Capture debug data
        if (byte_idx < PSX_DBG_MAX) {
            dbg_cmd[byte_idx] = rx;
            dbg_dat[byte_idx] = tx;
        }

        // ACK after every byte except the last
        if (byte_idx < len - 1) {
            send_ack();
        }
    }

done:
    // Ensure DAT is released (idle HIGH)
    GPIO.out_w1ts = DAT_MASK;
    spi_byte_count = spi_byte_count + bytes_done;
    dbg_len = (bytes_done < PSX_DBG_MAX) ? bytes_done : PSX_DBG_MAX;
}

// ── Public API ───────────────────────────────────────────────────

void psx_spi_init() {
    // ── Configure GPIO pins ──

    // CLK (input from console) — pull-up: PSX Mode 3 idles CLK HIGH
    gpio_set_direction((gpio_num_t)PIN_PSX_CLK, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)PIN_PSX_CLK, GPIO_PULLUP_ONLY);

    // CMD / MOSI (input from console) — pull-up for idle-HIGH
    gpio_set_direction((gpio_num_t)PIN_PSX_CMD, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)PIN_PSX_CMD, GPIO_PULLUP_ONLY);

    // DAT / MISO (output to console, open-drain — console provides pull-up)
    gpio_set_direction((gpio_num_t)PIN_PSX_DAT, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_level((gpio_num_t)PIN_PSX_DAT, 1);  // Idle HIGH (released)

    // ATT / CS (input from console)
    gpio_set_direction((gpio_num_t)PIN_PSX_ATT, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)PIN_PSX_ATT, GPIO_PULLUP_ONLY);

    // ACK (output to console, open-drain — console provides pull-up)
    gpio_set_direction((gpio_num_t)PIN_PSX_ACK, GPIO_MODE_OUTPUT_OD);
    gpio_set_level((gpio_num_t)PIN_PSX_ACK, 1);  // Idle HIGH

    // ── Register ATT GPIO interrupt (falling edge only) ──
    // Start DISABLED — call psx_spi_enable() when console is powered.
    gpio_set_intr_type((gpio_num_t)PIN_PSX_ATT, GPIO_INTR_DISABLE);
    gpio_isr_handler_add((gpio_num_t)PIN_PSX_ATT, att_isr, nullptr);

    // Initialize pending buffer with idle response
    pending_buf[0] = PSX_IDLE_BYTE;
    ControllerState idle;
    pending_len = 1 + psx_build_response(idle, 0, &pending_buf[1]);
    memcpy(response_buf, pending_buf, pending_len);

    Serial.printf("[psx_spi] initialized (bit-bang) — pins CLK=%d CMD=%d DAT=%d ATT=%d ACK=%d\n",
                  PIN_PSX_CLK, PIN_PSX_CMD, PIN_PSX_DAT, PIN_PSX_ATT, PIN_PSX_ACK);
}

void psx_spi_enable() {
    gpio_set_intr_type((gpio_num_t)PIN_PSX_ATT, GPIO_INTR_NEGEDGE);
    Serial.println("[psx_spi] enabled — listening for ATT");
}

void psx_spi_disable() {
    gpio_set_intr_type((gpio_num_t)PIN_PSX_ATT, GPIO_INTR_DISABLE);
    Serial.println("[psx_spi] disabled");
}

void psx_spi_cycle_clock() {
    Serial.println("[psx_spi] clock cycling not applicable in bit-bang mode");
}

void psx_spi_read_counters(uint32_t* att_falls, uint32_t* spi_bytes) {
    *att_falls = att_fall_count;
    *spi_bytes = spi_byte_count;
    att_fall_count = 0;
    spi_byte_count = 0;
}

void psx_spi_read_last_transaction(uint8_t* cmd_out, uint8_t* dat_out, uint8_t* len_out) {
    *len_out = dbg_len;
    for (uint8_t i = 0; i < dbg_len && i < PSX_DBG_MAX; i++) {
        cmd_out[i] = dbg_cmd[i];
        dat_out[i] = dbg_dat[i];
    }
}

void psx_spi_read_diag(bool* clk_level, bool* spi_usr_armed, uint32_t* slave_reg) {
    *clk_level = gpio_get_level((gpio_num_t)PIN_PSX_CLK);
    *spi_usr_armed = false;  // No SPI peripheral in bit-bang mode
    *slave_reg = 0;
}

void psx_spi_set_state(const ControllerState& cs, uint8_t console_mode) {
    pending_buf[0] = PSX_IDLE_BYTE;
    uint8_t len = 1 + psx_build_response(cs, console_mode, &pending_buf[1]);

    portENTER_CRITICAL(&response_mux);
    pending_len = len;
    portEXIT_CRITICAL(&response_mux);
}
