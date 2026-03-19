#include "display.h"
#include "config.h"
#include "menu.h"

#include <Arduino.h>
#include <U8g2lib.h>

// ── U8g2 Instance ──────────────────────────────────────────────────
// Full-buffer mode (_F_) uses 1KB RAM (128x64/8). HW I2C with
// explicit pin assignment for ESP32 (SDA=21, SCL=22).
static U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(
    U8G2_R0, U8X8_PIN_NONE, PIN_I2C_SCL, PIN_I2C_SDA
);

// ── Display State ──────────────────────────────────────────────────
static Screen   current_screen = SCREEN_SPLASH;
static uint32_t last_frame_ms  = 0;

// Visualizer data (pushed by main loop)
static ControllerState vis_ctrl;

// ── Screen Renderers ───────────────────────────────────────────────

static void render_pairing() {
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 10, "Waiting for");
    u8g2.drawStr(0, 22, "DualSense...");
    u8g2.drawHLine(0, 26, OLED_WIDTH);
    u8g2.drawStr(0, 42, "Hold SHARE + PS");
    u8g2.drawStr(0, 54, "to pair controller");
}

// ── Visualizer Drawing Helpers ────────────────────────────────────

// Draw a text-label button (L1, L2, R1, R2, SL, ST).
// Pressed = white rounded-box with black text (inverted).
static void draw_text_btn(uint8_t x, uint8_t y, const char* label, bool pressed) {
    uint8_t w = u8g2.getStrWidth(label);
    if (pressed) {
        u8g2.setDrawColor(1);
        u8g2.drawRBox(x - 1, y - 7, w + 3, 9, 1);
        u8g2.setDrawColor(0);
        u8g2.drawStr(x, y, label);
        u8g2.setDrawColor(1);
    } else {
        u8g2.drawStr(x, y, label);
    }
}

// Draw a face button: circle outline + symbol inside.
// Pressed = filled disc with symbol drawn in black.
static void draw_face_btn_triangle(uint8_t cx, uint8_t cy, uint8_t r, bool pressed) {
    if (pressed) {
        u8g2.drawDisc(cx, cy, r);
        u8g2.setDrawColor(0);
    } else {
        u8g2.drawCircle(cx, cy, r);
    }
    // Small triangle pointing up
    u8g2.drawTriangle(cx, cy - 3, cx - 3, cy + 2, cx + 3, cy + 2);
    u8g2.setDrawColor(1);
}

static void draw_face_btn_cross(uint8_t cx, uint8_t cy, uint8_t r, bool pressed) {
    if (pressed) {
        u8g2.drawDisc(cx, cy, r);
        u8g2.setDrawColor(0);
    } else {
        u8g2.drawCircle(cx, cy, r);
    }
    // X shape
    u8g2.drawLine(cx - 2, cy - 2, cx + 2, cy + 2);
    u8g2.drawLine(cx + 2, cy - 2, cx - 2, cy + 2);
    u8g2.setDrawColor(1);
}

static void draw_face_btn_circle(uint8_t cx, uint8_t cy, uint8_t r, bool pressed) {
    if (pressed) {
        u8g2.drawDisc(cx, cy, r);
        u8g2.setDrawColor(0);
    } else {
        u8g2.drawCircle(cx, cy, r);
    }
    u8g2.drawCircle(cx, cy, 2);
    u8g2.setDrawColor(1);
}

static void draw_face_btn_square(uint8_t cx, uint8_t cy, uint8_t r, bool pressed) {
    if (pressed) {
        u8g2.drawDisc(cx, cy, r);
        u8g2.setDrawColor(0);
    } else {
        u8g2.drawCircle(cx, cy, r);
    }
    u8g2.drawFrame(cx - 2, cy - 2, 5, 5);
    u8g2.setDrawColor(1);
}

// Draw the D-pad as a cross shape. Each arm fills when its direction is pressed.
static void draw_dpad(uint8_t cx, uint8_t cy,
                      bool up, bool down, bool left, bool right) {
    const uint8_t arm_w = 5;  // Width of each arm
    const uint8_t arm_l = 5;  // Length of each arm

    // Center block (always filled)
    u8g2.drawBox(cx - arm_w / 2, cy - arm_w / 2, arm_w, arm_w);

    // Up arm
    if (up) u8g2.drawBox(cx - arm_w / 2, cy - arm_w / 2 - arm_l, arm_w, arm_l);
    else    u8g2.drawFrame(cx - arm_w / 2, cy - arm_w / 2 - arm_l, arm_w, arm_l);

    // Down arm
    if (down) u8g2.drawBox(cx - arm_w / 2, cy + arm_w / 2 + 1, arm_w, arm_l);
    else      u8g2.drawFrame(cx - arm_w / 2, cy + arm_w / 2 + 1, arm_w, arm_l);

    // Left arm
    if (left) u8g2.drawBox(cx - arm_w / 2 - arm_l, cy - arm_w / 2, arm_l, arm_w);
    else      u8g2.drawFrame(cx - arm_w / 2 - arm_l, cy - arm_w / 2, arm_l, arm_w);

    // Right arm
    if (right) u8g2.drawBox(cx + arm_w / 2 + 1, cy - arm_w / 2, arm_l, arm_w);
    else       u8g2.drawFrame(cx + arm_w / 2 + 1, cy - arm_w / 2, arm_l, arm_w);
}

// Compute PS1 active-low button bytes from controller state.
// Byte layout matches PS1 digital pad protocol (0x41 device).
static void compute_ps1_bytes(const ControllerState& cs,
                              uint8_t& lo, uint8_t& hi) {
    // buttons_lo: SEL  n/a  n/a  STRT UP   RT   DN   LT
    lo = 0xFF;
    if (cs.select) lo &= ~(1 << 0);
    if (cs.start)  lo &= ~(1 << 3);
    if (cs.up)     lo &= ~(1 << 4);
    if (cs.right)  lo &= ~(1 << 5);
    if (cs.down)   lo &= ~(1 << 6);
    if (cs.left)   lo &= ~(1 << 7);

    // buttons_hi: L2   R2   L1   R1   TRI  CIR  X    SQ
    hi = 0xFF;
    if (cs.l2)       hi &= ~(1 << 0);
    if (cs.r2)       hi &= ~(1 << 1);
    if (cs.l1)       hi &= ~(1 << 2);
    if (cs.r1)       hi &= ~(1 << 3);
    if (cs.triangle) hi &= ~(1 << 4);
    if (cs.circle)   hi &= ~(1 << 5);
    if (cs.cross)    hi &= ~(1 << 6);
    if (cs.square)   hi &= ~(1 << 7);
}

// ── Visualizer Screen ────────────────────────────────────────────

static void render_visualizer() {
    // Status line
    u8g2.setFont(u8g2_font_5x7_tr);
    if (vis_ctrl.connected) {
        u8g2.drawStr(0, 7, "Connected");
    } else {
        u8g2.drawStr(0, 7, "No Controller");
    }

    // Player number (right-aligned)
    char pstr[4];
    snprintf(pstr, sizeof(pstr), "P%u", menu_get_player_number());
    uint8_t pw = u8g2.getStrWidth(pstr);
    u8g2.drawStr(OLED_WIDTH - pw, 7, pstr);

    // Separator
    u8g2.drawHLine(0, 10, OLED_WIDTH);

    // Shoulder buttons
    draw_text_btn(2,   21, "L2", vis_ctrl.l2);
    draw_text_btn(20,  21, "L1", vis_ctrl.l1);
    draw_text_btn(95,  21, "R1", vis_ctrl.r1);
    draw_text_btn(113, 21, "R2", vis_ctrl.r2);

    // D-pad
    draw_dpad(20, 38, vis_ctrl.up, vis_ctrl.down,
              vis_ctrl.left, vis_ctrl.right);

    // Select / Start
    draw_text_btn(50, 41, "SL", vis_ctrl.select);
    draw_text_btn(68, 41, "ST", vis_ctrl.start);

    // Face buttons
    draw_face_btn_triangle(108, 29, 5, vis_ctrl.triangle);
    draw_face_btn_square(96,    39, 5, vis_ctrl.square);
    draw_face_btn_circle(120,   39, 5, vis_ctrl.circle);
    draw_face_btn_cross(108,    49, 5, vis_ctrl.cross);

    // PS1 protocol bytes
    uint8_t lo, hi;
    compute_ps1_bytes(vis_ctrl, lo, hi);
    char hex[16];
    snprintf(hex, sizeof(hex), "PS1: %02X %02X", lo, hi);
    u8g2.drawStr(0, 62, hex);
}

// ── Settings Layout Constants ──────────────────────────────────────
static const uint8_t SETTINGS_MAX_VISIBLE = 4;
static const uint8_t SETTINGS_ITEM_H      = 12;
static const uint8_t SETTINGS_LIST_Y      = 16;
static const uint8_t SETTINGS_VAL_COL     = 95;  // Value column X position
static const uint8_t SETTINGS_VAL_EDIT_COL = 90; // Editing column (wider for brackets)

static void render_settings() {
    uint8_t count    = menu_get_item_count();
    uint8_t editable = menu_get_editable_count();
    uint8_t selected = menu_get_selected_item();
    bool    editing  = (menu_get_state() == MENU_SETTING_EDIT);

    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 10, "Settings");
    u8g2.drawHLine(0, 13, OLED_WIDTH);

    // Scroll offset keeps selected item visible.
    // Self-corrects on menu re-entry: selected resets to 0 in
    // handle_home, which triggers the `selected < scroll_offset`
    // guard below to reset scroll_offset on the first render.
    static uint8_t scroll_offset = 0;
    if (selected < scroll_offset) scroll_offset = selected;
    if (selected >= scroll_offset + SETTINGS_MAX_VISIBLE) {
        scroll_offset = selected - SETTINGS_MAX_VISIBLE + 1;
    }

    u8g2.setFont(u8g2_font_5x7_tr);
    for (uint8_t i = 0; i < SETTINGS_MAX_VISIBLE && (scroll_offset + i) < count; i++) {
        uint8_t idx = scroll_offset + i;
        uint8_t y = SETTINGS_LIST_Y + i * SETTINGS_ITEM_H;

        // Selection indicator
        if (idx == selected) {
            u8g2.drawStr(0, y + 8, ">");
        }

        const char* label = menu_get_item_label(idx);
        u8g2.drawStr(10, y + 8, label);

        // Show current value for editable items only
        if (idx < editable) {
            char val[8];
            if (idx == selected && editing) {
                menu_get_edit_value(val, sizeof(val));
                char decorated[sizeof(val) + 3]; // '[' + val + ']' + '\0'
                snprintf(decorated, sizeof(decorated), "[%s]", val);
                u8g2.drawStr(SETTINGS_VAL_EDIT_COL, y + 8, decorated);
            } else {
                menu_get_edit_value_for(idx, val, sizeof(val));
                u8g2.drawStr(SETTINGS_VAL_COL, y + 8, val);
            }
        }
    }
}

static void render_about() {
    u8g2.setFont(u8g2_font_helvB10_tr);
    int w = u8g2.getStrWidth(FW_NAME);
    u8g2.drawStr((OLED_WIDTH - w) / 2, 16, FW_NAME);

    u8g2.setFont(u8g2_font_6x10_tr);
    char ver[16];
    snprintf(ver, sizeof(ver), "v%s", FW_VERSION);
    w = u8g2.getStrWidth(ver);
    u8g2.drawStr((OLED_WIDTH - w) / 2, 32, ver);

    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(10, 48, "PS5-to-PS1 Bridge");
    u8g2.drawStr(10, 58, "[BAK] back");
}

static void render_menu() {
    MenuState ms = menu_get_state();
    switch (ms) {
        case MENU_SETTINGS:
        case MENU_SETTING_EDIT:
            render_settings();
            break;
        case MENU_ABOUT:
            render_about();
            break;
        default:
            break;
    }
}

// ── Public API ─────────────────────────────────────────────────────

void display_init() {
    u8g2.begin();
    u8g2.setContrast(200);
    Serial.println("[display] SH1106 initialized");
}

void display_show_splash() {
    current_screen = SCREEN_SPLASH;
    u8g2.clearBuffer();

    u8g2.setFont(u8g2_font_helvB12_tr);
    int w = u8g2.getStrWidth(FW_NAME);
    u8g2.drawStr((OLED_WIDTH - w) / 2, 30, FW_NAME);

    u8g2.setFont(u8g2_font_6x10_tr);
    char ver[16];
    snprintf(ver, sizeof(ver), "v%s", FW_VERSION);
    w = u8g2.getStrWidth(ver);
    u8g2.drawStr((OLED_WIDTH - w) / 2, 48, ver);

    u8g2.sendBuffer();
}

void display_set_screen(Screen screen) {
    current_screen = screen;
}

Screen display_get_screen() {
    return current_screen;
}

void display_set_controller(const ControllerState& state) {
    vis_ctrl = state;
}

void display_update() {
    uint32_t now = millis();
    if (now - last_frame_ms < DISPLAY_FRAME_MS) return;
    last_frame_ms = now;

    // Splash is a one-shot render — don't redraw
    if (current_screen == SCREEN_SPLASH) return;

    u8g2.clearBuffer();

    switch (current_screen) {
        case SCREEN_PAIRING:    render_pairing();    break;
        case SCREEN_VISUALIZER: render_visualizer(); break;
        case SCREEN_MENU:       render_menu();       break;
        default: break;
    }

    u8g2.sendBuffer();
}
