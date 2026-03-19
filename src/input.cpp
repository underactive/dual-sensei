#include "input.h"
#include "config.h"

#include <Arduino.h>
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

static void IRAM_ATTR encoder_isr(void* arg) {
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
// Shared ISR for all three buttons. The button index (0=CON, 1=BAK,
// 2=PHS) is passed via the void* arg. Debounce uses
// esp_timer_get_time() — no blocking delays in ISR context.

static volatile int64_t last_btn_time[3] = {0, 0, 0};

static const InputEvent BTN_EVENTS[3] DRAM_ATTR = {
    INPUT_BTN_CON,
    INPUT_BTN_BAK,
    INPUT_BTN_PHS,
};

static void IRAM_ATTR button_isr(void* arg) {
    uint32_t idx = (uint32_t)arg;
    int64_t now  = esp_timer_get_time();

    if (now - last_btn_time[idx] < DEBOUNCE_US) return;
    last_btn_time[idx] = now;

    InputEvent evt = BTN_EVENTS[idx];
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(input_queue, &evt, &woken);
    if (woken) portYIELD_FROM_ISR();
}

// ── Public API ─────────────────────────────────────────────────────

void input_init() {
    // Configure encoder pins with internal pull-ups
    pinMode(PIN_ENC_A,  INPUT_PULLUP);
    pinMode(PIN_ENC_B,  INPUT_PULLUP);
    pinMode(PIN_ENC_SW, INPUT_PULLUP);
    pinMode(PIN_BTN_CON, INPUT_PULLUP);
    pinMode(PIN_BTN_BAK, INPUT_PULLUP);

    input_queue = xQueueCreate(INPUT_QUEUE_SIZE, sizeof(InputEvent));

    // Read initial encoder state so the first transition is valid
    enc_prev_state = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);

    // Install GPIO ISR service (tolerate if Arduino already installed it)
    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        Serial.printf("[input] gpio_install_isr_service failed: %d\n", err);
    }

    // Encoder: interrupt on any edge of both channels
    gpio_set_intr_type((gpio_num_t)PIN_ENC_A, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type((gpio_num_t)PIN_ENC_B, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add((gpio_num_t)PIN_ENC_A, encoder_isr, nullptr);
    gpio_isr_handler_add((gpio_num_t)PIN_ENC_B, encoder_isr, nullptr);

    // Buttons: interrupt on falling edge (active-low, pulled high)
    gpio_set_intr_type((gpio_num_t)PIN_BTN_CON, GPIO_INTR_NEGEDGE);
    gpio_set_intr_type((gpio_num_t)PIN_BTN_BAK, GPIO_INTR_NEGEDGE);
    gpio_set_intr_type((gpio_num_t)PIN_ENC_SW,  GPIO_INTR_NEGEDGE);
    gpio_isr_handler_add((gpio_num_t)PIN_BTN_CON, button_isr, (void*)0);
    gpio_isr_handler_add((gpio_num_t)PIN_BTN_BAK, button_isr, (void*)1);
    gpio_isr_handler_add((gpio_num_t)PIN_ENC_SW,  button_isr, (void*)2);

    Serial.println("[input] initialized — encoder + 3 buttons");
}

InputEvent input_poll() {
    InputEvent evt = INPUT_NONE;
    xQueueReceive(input_queue, &evt, 0);  // Non-blocking
    return evt;
}

int32_t input_get_encoder_pos() {
    return encoder_position;
}
