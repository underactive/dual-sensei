#include "psx_spi.h"
#include "psx.h"
#include "config.h"

#include <Arduino.h>
#include "driver/gpio.h"
#include "driver/periph_ctrl.h"
#include "soc/spi_reg.h"
#include "soc/spi_periph.h"
#include "soc/gpio_sig_map.h"
#include "soc/gpio_struct.h"
#include "esp_intr_alloc.h"
#include "esp_rom_gpio.h"
#include "rom/ets_sys.h"

// ── SPI Peripheral ───────────────────────────────────────────────
// Using SPI3 (VSPI) in slave mode. Register-level access is required
// because the PSX protocol is byte-by-byte reactive — ESP-IDF's
// spi_slave driver adds too much latency for the ~12μs inter-byte window.

#define PSX_SPI 3  // SPI3 = VSPI

// ── Transaction State (ISR context) ──────────────────────────────

// Response buffer: [0xFF, ID, 0x5A, buttons_lo, buttons_hi, (RX, RY, LX, LY)]
static uint8_t DRAM_ATTR response_buf[PSX_RESPONSE_MAX + 1];  // +1 for leading 0xFF
static volatile uint8_t DRAM_ATTR response_len = 5;           // PS1 default (1 + 4)
static volatile uint8_t DRAM_ATTR byte_index = 0;
static volatile bool DRAM_ATTR transaction_active = false;

// Pending response (written by main loop, copied to response_buf on ATT fall)
static uint8_t pending_buf[PSX_RESPONSE_MAX + 1];
static volatile uint8_t pending_len = 5;
static portMUX_TYPE response_mux = portMUX_INITIALIZER_UNLOCKED;

// ISR handle for SPI interrupt
static intr_handle_t spi_intr_handle = nullptr;

// ── ACK Pulse ────────────────────────────────────────────────────
// Pull ACK LOW for PSX_ACK_PULSE_US after PSX_ACK_DELAY_US.
// Called from ISR after each byte except the last.

static inline void IRAM_ATTR send_ack() {
    ets_delay_us(PSX_ACK_DELAY_US);
    GPIO.out_w1tc = (1 << PIN_PSX_ACK);   // Drive LOW
    ets_delay_us(PSX_ACK_PULSE_US);
    GPIO.out_w1ts = (1 << PIN_PSX_ACK);   // Release (pull-up → HIGH)
}

// ── SPI ISR — fires after each 8-bit transfer ───────────────────

static void IRAM_ATTR psx_spi_isr(void* arg) {
    // Clear trans_done interrupt
    CLEAR_PERI_REG_MASK(SPI_SLAVE_REG(PSX_SPI), SPI_TRANS_DONE);

    if (!transaction_active) return;

    // Read received CMD byte (we don't use it in this phase, but clear the register)
    // uint8_t cmd = REG_READ(SPI_W0_REG(PSX_SPI)) & 0xFF;

    byte_index++;

    if (byte_index < response_len) {
        // Load next response byte into TX register
        WRITE_PERI_REG(SPI_W0_REG(PSX_SPI), response_buf[byte_index]);

        // Re-arm SPI for next byte
        SET_PERI_REG_MASK(SPI_CMD_REG(PSX_SPI), SPI_USR);

        // Send ACK unless this is the last byte
        if (byte_index < response_len - 1) {
            send_ack();
        }
    }
    // If byte_index >= response_len: transaction is done, no ACK, wait for ATT rise
}

// ── ATT GPIO ISR — transaction start/end ─────────────────────────

static void IRAM_ATTR att_isr(void* arg) {
    if (gpio_get_level((gpio_num_t)PIN_PSX_ATT) == 0) {
        // ATT fell — start of new transaction
        transaction_active = true;
        byte_index = 0;

        // Copy pending response to active buffer
        portENTER_CRITICAL_ISR(&response_mux);
        memcpy(response_buf, pending_buf, pending_len);
        response_len = pending_len;
        portEXIT_CRITICAL_ISR(&response_mux);

        // Load first byte (0xFF) and arm SPI
        WRITE_PERI_REG(SPI_W0_REG(PSX_SPI), response_buf[0]);
        SET_PERI_REG_MASK(SPI_CMD_REG(PSX_SPI), SPI_USR);

    } else {
        // ATT rose — end of transaction
        transaction_active = false;
        byte_index = 0;
    }
}

// ── Public API ───────────────────────────────────────────────────

void psx_spi_init() {
    // ── Enable SPI3 peripheral clock ──
    periph_module_enable(PERIPH_VSPI_MODULE);

    // ── Configure GPIO pins ──

    // CLK (input from console)
    gpio_set_direction((gpio_num_t)PIN_PSX_CLK, GPIO_MODE_INPUT);
    esp_rom_gpio_connect_in_signal(PIN_PSX_CLK, VSPICLK_IN_IDX, false);

    // CMD / MOSI (input from console)
    gpio_set_direction((gpio_num_t)PIN_PSX_CMD, GPIO_MODE_INPUT);
    esp_rom_gpio_connect_in_signal(PIN_PSX_CMD, VSPID_IN_IDX, false);

    // DAT / MISO (output to console, open-drain — console provides pull-up)
    gpio_set_direction((gpio_num_t)PIN_PSX_DAT, GPIO_MODE_INPUT_OUTPUT_OD);
    esp_rom_gpio_connect_out_signal(PIN_PSX_DAT, VSPIQ_OUT_IDX, false, false);

    // ATT / CS (input from console — routed to both SPI CS and GPIO ISR)
    gpio_set_direction((gpio_num_t)PIN_PSX_ATT, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)PIN_PSX_ATT, GPIO_PULLUP_ONLY);
    esp_rom_gpio_connect_in_signal(PIN_PSX_ATT, VSPICS0_IN_IDX, false);

    // ACK (output to console, open-drain — console provides pull-up)
    gpio_set_direction((gpio_num_t)PIN_PSX_ACK, GPIO_MODE_OUTPUT_OD);
    gpio_set_level((gpio_num_t)PIN_PSX_ACK, 1);  // Idle HIGH

    // ── Configure SPI3 registers for PSX slave mode ──

    // Reset SPI peripheral
    CLEAR_PERI_REG_MASK(SPI_SLAVE_REG(PSX_SPI), SPI_TRANS_DONE);
    SET_PERI_REG_MASK(SPI_SLAVE_REG(PSX_SPI), SPI_SLAVE_MODE);

    // Clock: PSX Mode 3 (CPOL=1, CPHA=1)
    // BlueRetro finding: ESP32 slave mode needs inverted edge settings
    CLEAR_PERI_REG_MASK(SPI_PIN_REG(PSX_SPI), SPI_CK_IDLE_EDGE);  // ck_idle_edge = 0
    SET_PERI_REG_MASK(SPI_USER_REG(PSX_SPI), SPI_CK_I_EDGE);      // ck_i_edge = 1

    // LSB first (PSX protocol)
    SET_PERI_REG_MASK(SPI_CTRL_REG(PSX_SPI), SPI_WR_BIT_ORDER);   // TX: LSB first
    SET_PERI_REG_MASK(SPI_CTRL_REG(PSX_SPI), SPI_RD_BIT_ORDER);   // RX: LSB first

    // Full duplex: enable both MOSI (receive CMD) and MISO (send DAT)
    SET_PERI_REG_MASK(SPI_USER_REG(PSX_SPI), SPI_USR_MOSI | SPI_USR_MISO | SPI_DOUTDIN);

    // Disable command/address/dummy phases (raw data exchange only)
    CLEAR_PERI_REG_MASK(SPI_USER_REG(PSX_SPI), SPI_USR_COMMAND | SPI_USR_ADDR | SPI_USR_DUMMY);

    // 8-bit transfers (bit length = 7 means 8 bits)
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(PSX_SPI), 7);  // RX: 8 bits
    WRITE_PERI_REG(SPI_MISO_DLEN_REG(PSX_SPI), 7);  // TX: 8 bits

    // Pre-load first response byte (0xFF = hi-Z / idle)
    WRITE_PERI_REG(SPI_W0_REG(PSX_SPI), PSX_IDLE_BYTE);

    // ── Register SPI interrupt ──
    // Enable trans_done interrupt: bit 0 of INT_EN field (bit 5 of SPI_SLAVE_REG)
    SET_PERI_REG_BITS(SPI_SLAVE_REG(PSX_SPI), SPI_INT_EN_V, 0x01, SPI_INT_EN_S);

    esp_intr_alloc(ETS_SPI3_INTR_SOURCE, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3,
                   psx_spi_isr, nullptr, &spi_intr_handle);

    // ── Register ATT GPIO interrupt (both edges) ──
    gpio_set_intr_type((gpio_num_t)PIN_PSX_ATT, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add((gpio_num_t)PIN_PSX_ATT, att_isr, nullptr);

    // Arm SPI for first transaction
    SET_PERI_REG_MASK(SPI_CMD_REG(PSX_SPI), SPI_USR);

    // Initialize pending buffer with idle response
    pending_buf[0] = PSX_IDLE_BYTE;
    ControllerState idle;
    pending_len = 1 + psx_build_response(idle, 0, &pending_buf[1]);
    memcpy(response_buf, pending_buf, pending_len);

    Serial.printf("[psx_spi] initialized — VSPI slave, pins CLK=%d CMD=%d DAT=%d ATT=%d ACK=%d\n",
                  PIN_PSX_CLK, PIN_PSX_CMD, PIN_PSX_DAT, PIN_PSX_ATT, PIN_PSX_ACK);
}

void psx_spi_set_state(const ControllerState& cs, uint8_t console_mode) {
    // Build response in pending buffer (main loop context)
    pending_buf[0] = PSX_IDLE_BYTE;
    uint8_t len = 1 + psx_build_response(cs, console_mode, &pending_buf[1]);

    portENTER_CRITICAL(&response_mux);
    pending_len = len;
    portEXIT_CRITICAL(&response_mux);
}
