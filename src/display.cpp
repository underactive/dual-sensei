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
static int32_t vis_enc_pos = 0;
static bool    vis_btn_con = false;
static bool    vis_btn_bak = false;
static bool    vis_btn_phs = false;

// ── Screen Renderers ───────────────────────────────────────────────

static void render_pairing() {
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 10, "Waiting for");
    u8g2.drawStr(0, 22, "DualSense...");
    u8g2.drawHLine(0, 26, OLED_WIDTH);
    u8g2.drawStr(0, 42, "Hold SHARE + PS");
    u8g2.drawStr(0, 54, "to pair controller");
}

static void render_visualizer() {
    char buf[22];
    u8g2.setFont(u8g2_font_6x10_tr);

    u8g2.drawStr(0, 10, "Input Test");
    u8g2.drawHLine(0, 13, OLED_WIDTH);

    snprintf(buf, sizeof(buf), "Encoder: %ld", (long)vis_enc_pos);
    u8g2.drawStr(0, 28, buf);

    snprintf(buf, sizeof(buf), "CON:%d BAK:%d PHS:%d",
             vis_btn_con, vis_btn_bak, vis_btn_phs);
    u8g2.drawStr(0, 42, buf);

    u8g2.drawHLine(0, 46, OLED_WIDTH);
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 57, "[CON] open menu");
}

static void render_settings() {
    uint8_t count    = menu_get_item_count();
    uint8_t selected = menu_get_selected_item();
    bool    editing  = (menu_get_state() == MENU_SETTING_EDIT);

    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 10, "Settings");
    u8g2.drawHLine(0, 13, OLED_WIDTH);

    // Render visible menu items (up to 4 fit below the header)
    static const uint8_t MAX_VISIBLE = 4;
    static const uint8_t ITEM_H = 12;
    static const uint8_t LIST_Y = 16;

    // Scroll offset keeps selected item visible
    static uint8_t scroll_offset = 0;
    if (selected < scroll_offset) scroll_offset = selected;
    if (selected >= scroll_offset + MAX_VISIBLE) {
        scroll_offset = selected - MAX_VISIBLE + 1;
    }

    u8g2.setFont(u8g2_font_5x7_tr);
    for (uint8_t i = 0; i < MAX_VISIBLE && (scroll_offset + i) < count; i++) {
        uint8_t idx = scroll_offset + i;
        uint8_t y = LIST_Y + i * ITEM_H;

        // Selection indicator
        if (idx == selected) {
            u8g2.drawStr(0, y + 8, ">");
        }

        const char* label = menu_get_item_label(idx);
        u8g2.drawStr(10, y + 8, label);

        // Show current value for editable items (first 3)
        if (idx < 3) {
            char val[8];
            if (idx == selected && editing) {
                // Editing: show value with brackets
                menu_get_edit_value(val, sizeof(val));
                char decorated[12];
                snprintf(decorated, sizeof(decorated), "[%s]", val);
                u8g2.drawStr(90, y + 8, decorated);
            } else {
                // Just show the value
                menu_get_edit_value_for(idx, val, sizeof(val));
                u8g2.drawStr(95, y + 8, val);
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

void display_set_encoder_pos(int32_t pos) {
    vis_enc_pos = pos;
}

void display_set_button_states(bool con, bool bak, bool phs) {
    vis_btn_con = con;
    vis_btn_bak = bak;
    vis_btn_phs = phs;
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
