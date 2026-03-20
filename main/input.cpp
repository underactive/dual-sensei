#include "input.h"
#include "config.h"

#include <Arduino.h>
// WHY: ESP-IDF gpio API used instead of Arduino attachInterrupt because
// attachInterrupt does not support passing an argument (void*) to the ISR.
// This is needed for the shared button_isr to identify which button fired.
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// ── Quadrature Encoder State Machine ───────────────────────────────
//
// The encoder produces Gray-coded outputs on channels A and B.
// Each physical detent traverses 4 state transitions:
//   CW:  00 → 01 → 11 → 10 → 00
//   CCW: 00 → 10 → 11 → 01 → 00
//
// This table maps (prev_state << 2 | curr_state) → direction:
//   +1 = CW transition, -1 = CCW transition, 0 = invalid/no-move
//
static const int8_t ENC_TABLE[16] DRAM_ATTR = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

static volatile uint8_t enc_prev_state = 0;
static volatile int8_t  enc_accum      = 0;
static volatile int32_t encoder_position = 0;

static QueueHandle_t input_queue = nullptr;

// ── Encoder ISR ────────────────────────────────────────────────────
// Fires on any edge of either encoder channel. The state machine
// inherently rejects noise (invalid transitions produce delta=0),
// so no delay-based debounce is needed.
//
// CONCURRENCY NOTE: Both PIN_ENC_A and PIN_ENC_B are registered to
// this same ISR via gpio_isr_handler_add. On ESP32, the GPIO ISR
// service multiplexes all GPIO interrupts into a single ISR at a
// fixed priority, so both handlers run serialized — no reentrancy
// or cross-core race on enc_accum/enc_prev_state.

static void IRAM_ATTR encoder_isr(void* arg) {
    if (!input_queue) return;

    uint8_t a   = gpio_get_level((gpio_num_t)PIN_ENC_A);
    uint8_t b   = gpio_get_level((gpio_num_t)PIN_ENC_B);
    uint8_t cur = (a << 1) | b;
    int8_t delta = ENC_TABLE[(enc_prev_state << 2) | cur];
    enc_prev_state = cur;

    if (delta == 0) return;
    enc_accum += delta;

    InputEvent evt;
    if (enc_accum >= ENC_STEPS_PER_DETENT) {
        enc_accum -= ENC_STEPS_PER_DETENT;
        encoder_position++;
        evt = INPUT_ENC_CW;
    } else if (enc_accum <= -ENC_STEPS_PER_DETENT) {
        enc_accum += ENC_STEPS_PER_DETENT;
        encoder_position--;
        evt = INPUT_ENC_CCW;
    } else {
        return;
    }

    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(input_queue, &evt, &woken);
    if (woken) portYIELD_FROM_ISR();
}

// ── Button ISR ─────────────────────────────────────────────────────
// Shared ISR for all three buttons. The button index is passed via
// the void* arg:  0=CON, 1=BAK, 2=PHS
// Debounce uses esp_timer_get_time() — no blocking delays in ISR.

static const uint8_t BTN_COUNT = 3;
static volatile int64_t last_btn_time[BTN_COUNT] = {0, 0, 0};

static const InputEvent BTN_EVENTS[BTN_COUNT] DRAM_ATTR = {
    INPUT_BTN_CON,
    INPUT_BTN_BAK,
    INPUT_BTN_PHS,
};

// Pin lookup for level confirmation inside ISR
static const gpio_num_t BTN_PINS[BTN_COUNT] DRAM_ATTR = {
    (gpio_num_t)PIN_BTN_CON,
    (gpio_num_t)PIN_BTN_BAK,
    (gpio_num_t)PIN_ENC_SW,
};

// Armed/disarmed state: a button is disarmed after it fires an event
// and can only re-arm when the main loop confirms the pin is HIGH
// (released) for at least DEBOUNCE_US. This prevents release bounce
// from generating phantom press events regardless of hold duration.
static volatile bool btn_armed[BTN_COUNT] = {true, true, true};

static void IRAM_ATTR button_isr(void* arg) {
    if (!input_queue) return;

    uint32_t idx = (uint32_t)(uintptr_t)arg;
    if (idx >= BTN_COUNT) return;

    int64_t now = esp_timer_get_time();
    if (now - last_btn_time[idx] < DEBOUNCE_US) return;
    last_btn_time[idx] = now;

    // Disarmed — button is in the press/release bounce cycle.
    // Still update last_btn_time above so bounce edges push back
    // the re-arm window in input_poll().
    if (!btn_armed[idx]) return;

    // Confirm pin is LOW (active press)
    if (gpio_get_level(BTN_PINS[idx])) return;

    btn_armed[idx] = false;  // Disarm until main loop confirms release
    InputEvent evt = BTN_EVENTS[idx];
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(input_queue, &evt, &woken);
    if (woken) portYIELD_FROM_ISR();
}

// ── Public API ─────────────────────────────────────────────────────

void input_init() {
    // Clean up prior init if called more than once (e.g., soft reset)
    if (input_queue) {
        gpio_isr_handler_remove((gpio_num_t)PIN_ENC_A);
        gpio_isr_handler_remove((gpio_num_t)PIN_ENC_B);
        gpio_isr_handler_remove((gpio_num_t)PIN_BTN_CON);
        gpio_isr_handler_remove((gpio_num_t)PIN_BTN_BAK);
        gpio_isr_handler_remove((gpio_num_t)PIN_ENC_SW);
        vQueueDelete(input_queue);
        input_queue = nullptr;
    }

    // Configure encoder pins with internal pull-ups
    pinMode(PIN_ENC_A,  INPUT_PULLUP);
    pinMode(PIN_ENC_B,  INPUT_PULLUP);
    pinMode(PIN_ENC_SW, INPUT_PULLUP);
    pinMode(PIN_BTN_CON, INPUT_PULLUP);
    pinMode(PIN_BTN_BAK, INPUT_PULLUP);

    input_queue = xQueueCreate(INPUT_QUEUE_SIZE, sizeof(InputEvent));
    if (!input_queue) {
        Serial.println("[input] FATAL: queue alloc failed");
        abort();
    }

    // Read initial encoder state so the first transition is valid
    enc_prev_state = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
    enc_accum = 0;
    encoder_position = 0;

    // Install GPIO ISR service (tolerate if Arduino already installed it)
    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        Serial.printf("[input] FATAL: gpio_install_isr_service failed: %d\n", err);
        abort();
    }

    // Encoder: interrupt on any edge of both channels
    gpio_set_intr_type((gpio_num_t)PIN_ENC_A, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type((gpio_num_t)PIN_ENC_B, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add((gpio_num_t)PIN_ENC_A, encoder_isr, nullptr);
    gpio_isr_handler_add((gpio_num_t)PIN_ENC_B, encoder_isr, nullptr);

    // Buttons: interrupt on any edge (both press and release).
    // Rising edges (release) update the debounce timer so that
    // release-bounce falling edges are rejected by the time gate.
    gpio_set_intr_type((gpio_num_t)PIN_BTN_CON, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type((gpio_num_t)PIN_BTN_BAK, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type((gpio_num_t)PIN_ENC_SW,  GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add((gpio_num_t)PIN_BTN_CON, button_isr, (void*)(uintptr_t)0);
    gpio_isr_handler_add((gpio_num_t)PIN_BTN_BAK, button_isr, (void*)(uintptr_t)1);
    gpio_isr_handler_add((gpio_num_t)PIN_ENC_SW,  button_isr, (void*)(uintptr_t)2);

    Serial.println("[input] initialized — encoder + 3 buttons");
}

InputEvent input_poll() {
    // Re-arm buttons whose pins have settled HIGH (released) after
    // the debounce window. This is the ONLY place buttons re-arm —
    // the ISR disarms on press and never re-arms, guaranteeing one
    // event per physical press regardless of bounce characteristics.
    int64_t now = esp_timer_get_time();
    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        if (!btn_armed[i] &&
            gpio_get_level(BTN_PINS[i]) &&
            (now - last_btn_time[i] >= DEBOUNCE_US)) {
            btn_armed[i] = true;
        }
    }

    InputEvent evt = INPUT_NONE;
    xQueueReceive(input_queue, &evt, 0);  // Non-blocking
    return evt;
}

int32_t input_get_encoder_pos() {
    return encoder_position;
}

void input_flush_queue() {
    if (!input_queue) return;
    xQueueReset(input_queue);
}
