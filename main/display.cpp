#include "display.h"
#include "bt.h"
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

    if (bt_is_connected()) {
        u8g2.drawStr(0, 10, "Controller");
        u8g2.drawStr(0, 22, "Connected!");
        u8g2.drawHLine(0, 26, OLED_WIDTH);
        u8g2.setFont(u8g2_font_5x7_tr);
        u8g2.drawStr(0, 42, "Press [BAK] to return");
    } else {
        u8g2.drawStr(0, 10, "Waiting for");
        u8g2.drawStr(0, 22, "controller...");
        u8g2.drawHLine(0, 26, OLED_WIDTH);
        u8g2.setFont(u8g2_font_5x7_tr);
        u8g2.drawStr(0, 38, "PS4/5: Share+PS hold");
        u8g2.drawStr(0, 48, "Xbox:  pair btn 3s");
        u8g2.drawStr(0, 58, "Switch: sync btn");
    }
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

// Draw an analog stick: circle outline with a centered position dot.
static void draw_analog_stick(uint8_t cx, uint8_t cy, uint8_t r,
                              uint8_t axis_x, uint8_t axis_y) {
    u8g2.drawCircle(cx, cy, r);

    // Position dot: map 0-255 axis to -(r-1)..+(r-1) pixel offset
    int8_t dx = (int8_t)(((int16_t)axis_x - 128) * (r - 1) / 127);
    int8_t dy = (int8_t)(((int16_t)axis_y - 128) * (r - 1) / 127);
    // drawDisc r=1 gives a 5-pixel diamond centered exactly on the target pixel
    u8g2.drawDisc(cx + dx, cy + dy, 1);
}

// Compute PSX active-low protocol bytes from controller state.
// PS1 mode: 2 bytes (button_lo, button_hi).
// PS2 mode: 6 bytes (button_lo, button_hi, RX, RY, LX, LY).
static void compute_protocol_bytes(const ControllerState& cs, uint8_t console_mode,
                                   uint8_t* bytes, uint8_t& count) {
    // buttons_lo: LT  DN  RT  UP  STRT [R3] [L3] SEL  (active-low)
    // PS1: bits 1,2 always 1 (unused per digital pad spec). PS2: L3=bit1, R3=bit2.
    uint8_t lo = 0xFF;
    if (cs.select) lo &= ~(1 << 0);
    if (cs.start)  lo &= ~(1 << 3);
    if (cs.up)     lo &= ~(1 << 4);
    if (cs.right)  lo &= ~(1 << 5);
    if (cs.down)   lo &= ~(1 << 6);
    if (cs.left)   lo &= ~(1 << 7);

    if (console_mode == 1) {
        if (cs.l3) lo &= ~(1 << 1);
        if (cs.r3) lo &= ~(1 << 2);
    }

    // buttons_hi: SQ  X  CIR  TRI  R1  L1  R2  L2  (active-low)
    uint8_t hi = 0xFF;
    if (cs.l2)       hi &= ~(1 << 0);
    if (cs.r2)       hi &= ~(1 << 1);
    if (cs.l1)       hi &= ~(1 << 2);
    if (cs.r1)       hi &= ~(1 << 3);
    if (cs.triangle) hi &= ~(1 << 4);
    if (cs.circle)   hi &= ~(1 << 5);
    if (cs.cross)    hi &= ~(1 << 6);
    if (cs.square)   hi &= ~(1 << 7);

    bytes[0] = lo;
    bytes[1] = hi;

    if (console_mode == 1) {  // PS2: append 4 stick bytes (RX, RY, LX, LY)
        bytes[2] = cs.rx;
        bytes[3] = cs.ry;
        bytes[4] = cs.lx;
        bytes[5] = cs.ly;
        count = 6;
    } else {
        count = 2;
    }
}

// ── Status Line Icons ────────────────────────────────────────────

// Tiny gamepad icon (7x5 pixels). Body on top, grips on bottom.
static void draw_icon_gamepad(uint8_t x, uint8_t y) {
    u8g2.drawFrame(x, y, 7, 3);        // Body
    u8g2.drawPixel(x + 2, y + 1);      // Left stick
    u8g2.drawPixel(x + 4, y + 1);      // Right stick
    u8g2.drawBox(x, y + 3, 2, 2);      // Left grip
    u8g2.drawBox(x + 5, y + 3, 2, 2);  // Right grip
}

// Tiny console icon (7x5 pixels). Rectangle with disc drive slot.
static void draw_icon_console(uint8_t x, uint8_t y) {
    u8g2.drawFrame(x, y, 7, 5);        // Console body
    u8g2.drawHLine(x + 2, y + 2, 3);   // Disc drive slot
}

// ── Visualizer Screen ────────────────────────────────────────────

static void render_visualizer() {
    uint8_t mode = menu_get_console_mode();

    // ── Status line: P1  [gamepad] Name  [console] PS2 ──
    u8g2.setFont(u8g2_font_5x7_tr);

    // Left: player number
    char pstr[4];
    snprintf(pstr, sizeof(pstr), "P%u", menu_get_player_number());
    u8g2.drawStr(0, 7, pstr);
    uint8_t px_end = u8g2.getStrWidth(pstr);

    // Right: console icon + mode
    const char* mode_str = (mode == 1) ? "PS2" : "PS1";
    uint8_t mw = u8g2.getStrWidth(mode_str);
    u8g2.drawStr(OLED_WIDTH - mw, 7, mode_str);
    uint8_t console_ix = OLED_WIDTH - mw - 9;  // icon(7) + gap(2)
    draw_icon_console(console_ix, 2);

    // Center: controller icon + name (or "No Controller")
    if (vis_ctrl.connected) {
        const char* name = bt_get_controller_name();
        if (name[0] == '\0') name = "Controller";
        uint8_t nw = u8g2.getStrWidth(name);
        uint8_t total = 7 + 2 + nw;  // icon(7) + gap(2) + name
        uint8_t cx = (px_end + 3 + console_ix) / 2 - total / 2;
        draw_icon_gamepad(cx, 2);
        u8g2.drawStr(cx + 9, 7, name);
    } else {
        const char* nc = "---";
        uint8_t ncw = u8g2.getStrWidth(nc);
        uint8_t cx = (px_end + 3 + console_ix) / 2 - ncw / 2;
        u8g2.drawStr(cx, 7, nc);
    }

    u8g2.drawHLine(0, 10, OLED_WIDTH);

    // ── Shoulder row ──
    if (mode == 1) {
        // PS2: L1 L2 L3 ... R3 R2 R1 (outside-in)
        draw_text_btn(2,   21, "L1", vis_ctrl.l1);
        draw_text_btn(16,  21, "L2", vis_ctrl.l2);
        draw_text_btn(30,  21, "L3", vis_ctrl.l3);
        draw_text_btn(85,  21, "R3", vis_ctrl.r3);
        draw_text_btn(99,  21, "R2", vis_ctrl.r2);
        draw_text_btn(113, 21, "R1", vis_ctrl.r1);
    } else {
        // PS1: L1 L2 ... R2 R1 (outside-in)
        draw_text_btn(2,   21, "L1", vis_ctrl.l1);
        draw_text_btn(20,  21, "L2", vis_ctrl.l2);
        draw_text_btn(95,  21, "R2", vis_ctrl.r2);
        draw_text_btn(113, 21, "R1", vis_ctrl.r1);
    }

    // ── D-pad ──
    draw_dpad(20, 38, vis_ctrl.up, vis_ctrl.down,
              vis_ctrl.left, vis_ctrl.right);

    // ── Select / Start ──
    draw_text_btn(50, 37, "SL", vis_ctrl.select);
    draw_text_btn(68, 37, "ST", vis_ctrl.start);

    // ── Face buttons ──
    draw_face_btn_triangle(108, 29, 5, vis_ctrl.triangle);
    draw_face_btn_square(96,    39, 5, vis_ctrl.square);
    draw_face_btn_circle(120,   39, 5, vis_ctrl.circle);
    draw_face_btn_cross(108,    49, 5, vis_ctrl.cross);

    // ── Analog sticks (PS2 only) ──
    if (mode == 1) {
        draw_analog_stick(48, 45, 6, vis_ctrl.lx, vis_ctrl.ly);
        draw_analog_stick(80, 45, 6, vis_ctrl.rx, vis_ctrl.ry);
    }

    // ── Protocol bytes ──
    uint8_t bytes[6];
    uint8_t count = 0;
    compute_protocol_bytes(vis_ctrl, mode, bytes, count);
    char hex[32];
    if (count == 2) {
        snprintf(hex, sizeof(hex), "PS1: %02X %02X", bytes[0], bytes[1]);
    } else {
        snprintf(hex, sizeof(hex), "PS2:%02X%02X %02X%02X %02X%02X",
                 bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]);
    }
    u8g2.drawStr(0, 62, hex);
}

static void render_settings() {
    const MenuItem* items    = menu_get_items();
    uint8_t         count    = menu_get_item_count();
    uint8_t         selected = menu_get_selected_item();
    uint8_t         offset   = menu_get_scroll_offset();
    bool            editing  = menu_is_editing();

    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.setFontMode(1);  // Transparent — required for inverted text rendering

    // ── Header ──
    u8g2.setDrawColor(1);
    u8g2.drawStr(2, 7, "Settings");
    u8g2.drawHLine(0, 9, OLED_WIDTH);

    // ── Viewport: 5 rows x 8px starting at y=10 ──
    for (uint8_t row = 0; row < MENU_VIEWPORT_ROWS; row++) {
        uint8_t idx = offset + row;
        if (idx >= count) break;

        const MenuItem& item = items[idx];
        uint8_t row_y = MENU_VIEWPORT_Y + row * MENU_ROW_H;  // Top of row
        uint8_t text_y = row_y + 7;  // Baseline for 5x7 font

        bool is_selected = (idx == selected);

        switch (item.type) {
            case MENU_HEADING: {
                // Centered "- Label -" in normal color
                char hdr[24];
                snprintf(hdr, sizeof(hdr), "- %s -", item.label);
                uint8_t hw = u8g2.getStrWidth(hdr);
                u8g2.setDrawColor(1);
                u8g2.drawStr((OLED_WIDTH - hw) / 2, text_y, hdr);
                break;
            }
            case MENU_VALUE: {
                char val[8];
                menu_get_value_str(item.setting_id, val, sizeof(val));

                if (is_selected && editing) {
                    // Label in normal color
                    u8g2.setDrawColor(1);
                    u8g2.drawStr(2, text_y, item.label);

                    // Build the arrow+value string
                    char decorated[16];
                    bool at_min = menu_is_at_min();
                    bool at_max = menu_is_at_max();
                    snprintf(decorated, sizeof(decorated), "%s%s%s",
                             at_min ? "  " : "< ",
                             val,
                             at_max ? "  " : " >");

                    // Invert only the value+arrows area (right-aligned)
                    uint8_t dw = u8g2.getStrWidth(decorated);
                    uint8_t dx = OLED_WIDTH - dw - 2;
                    u8g2.drawBox(dx - 1, row_y, dw + 3, MENU_ROW_H);
                    u8g2.setDrawColor(0);
                    u8g2.drawStr(dx, text_y, decorated);
                } else if (is_selected) {
                    // Full row inversion
                    u8g2.setDrawColor(1);
                    u8g2.drawBox(0, row_y, OLED_WIDTH, MENU_ROW_H);
                    u8g2.setDrawColor(0);
                    u8g2.drawStr(2, text_y, item.label);
                    uint8_t vw = u8g2.getStrWidth(val);
                    u8g2.drawStr(OLED_WIDTH - vw - 2, text_y, val);
                } else {
                    // Normal
                    u8g2.setDrawColor(1);
                    u8g2.drawStr(2, text_y, item.label);
                    uint8_t vw = u8g2.getStrWidth(val);
                    u8g2.drawStr(OLED_WIDTH - vw - 2, text_y, val);
                }
                break;
            }
            case MENU_ACTION: {
                if (is_selected) {
                    // Full row inversion
                    u8g2.setDrawColor(1);
                    u8g2.drawBox(0, row_y, OLED_WIDTH, MENU_ROW_H);
                    u8g2.setDrawColor(0);
                    u8g2.drawStr(2, text_y, item.label);
                    u8g2.drawStr(OLED_WIDTH - 7, text_y, ">");
                } else {
                    u8g2.setDrawColor(1);
                    u8g2.drawStr(2, text_y, item.label);
                    u8g2.drawStr(OLED_WIDTH - 7, text_y, ">");
                }
                break;
            }
        }
    }

    // ── Bottom separator ──
    u8g2.setDrawColor(1);
    u8g2.drawHLine(0, MENU_VIEWPORT_Y + MENU_VIEWPORT_ROWS * MENU_ROW_H, OLED_WIDTH);

    // ── Help bar ──
    const char* help = nullptr;
    if (editing) {
        help = "Turn to adjust";
    } else if (selected < count && items[selected].help) {
        help = items[selected].help;
    }
    if (help) {
        u8g2.drawStr(2, MENU_HELP_BASELINE, help);
    }

    u8g2.setFontMode(0);  // Restore default (solid background)
    u8g2.setDrawColor(1);
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
    u8g2.drawStr(10, 48, "PS5-to-PSX Bridge");
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

// ── PNG Screenshot Encoder ────────────────────────────────────────
// Outputs a complete base64-encoded PNG of the 128x64 OLED framebuffer
// over Serial. Copy the base64 between the markers and decode to PNG.
// Ported from ghost_operator project.

static uint32_t png_crc32(const uint8_t* data, size_t len, uint32_t crc) {
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
    }
    return crc;
}

static const char PROGMEM b64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static uint8_t b64_buf[57];  // 57 raw bytes = 76 base64 chars per line
static uint8_t b64_pos;

static void b64_flush() {
    if (b64_pos == 0) return;
    char out[4];
    for (uint8_t i = 0; i < b64_pos; i += 3) {
        uint8_t rem = b64_pos - i;
        uint32_t tri = ((uint32_t)b64_buf[i] << 16);
        if (rem > 1) tri |= ((uint32_t)b64_buf[i + 1] << 8);
        if (rem > 2) tri |= b64_buf[i + 2];
        out[0] = pgm_read_byte(&b64_chars[(tri >> 18) & 0x3F]);
        out[1] = pgm_read_byte(&b64_chars[(tri >> 12) & 0x3F]);
        out[2] = (rem > 1) ? pgm_read_byte(&b64_chars[(tri >> 6) & 0x3F]) : '=';
        out[3] = (rem > 2) ? pgm_read_byte(&b64_chars[tri & 0x3F]) : '=';
        Serial.write((uint8_t*)out, 4);
    }
    Serial.println();
    b64_pos = 0;
}

static void b64_write(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        b64_buf[b64_pos++] = data[i];
        if (b64_pos == 57) b64_flush();
    }
}

static void b64_write_byte(uint8_t val) {
    b64_buf[b64_pos++] = val;
    if (b64_pos == 57) b64_flush();
}

static void b64_write_u32(uint32_t val) {
    uint8_t buf[4] = {
        (uint8_t)(val >> 24), (uint8_t)(val >> 16),
        (uint8_t)(val >> 8),  (uint8_t)(val)
    };
    b64_write(buf, 4);
}

// Convert one row from page format (U8g2/SSD1306/SH1106) to PNG scanline
static void convert_page_row(const uint8_t* fb, uint8_t y, uint8_t* out16) {
    uint8_t page = y >> 3;
    uint8_t bit  = y & 7;
    const uint8_t* page_base = fb + page * 128;
    for (uint8_t byte_idx = 0; byte_idx < 16; byte_idx++) {
        uint8_t packed = 0;
        uint8_t x = byte_idx << 3;
        for (uint8_t bit_pos = 0; bit_pos < 8; bit_pos++) {
            if (page_base[x + bit_pos] & (1 << bit))
                packed |= (0x80 >> bit_pos);
        }
        out16[byte_idx] = packed;
    }
}

void display_screenshot() {
    const uint8_t* fb = u8g2.getBufferPtr();
    uint8_t row_buf[16];

    Serial.println("\n--- PNG START ---");
    b64_pos = 0;

    // PNG signature
    static const uint8_t PROGMEM png_sig[] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
    };
    uint8_t sig[8];
    memcpy_P(sig, png_sig, 8);
    b64_write(sig, 8);

    // IHDR chunk: 128x64, 1-bit grayscale
    static const uint8_t PROGMEM ihdr[] = {
        0x00, 0x00, 0x00, 0x0D,  // length = 13
        0x49, 0x48, 0x44, 0x52,  // "IHDR"
        0x00, 0x00, 0x00, 0x80,  // width = 128
        0x00, 0x00, 0x00, 0x40,  // height = 64
        0x01,                    // bit depth = 1
        0x00,                    // color type = grayscale
        0x00, 0x00, 0x00,        // compression, filter, interlace
    };
    uint8_t ihdr_buf[21];
    memcpy_P(ihdr_buf, ihdr, 21);
    b64_write(ihdr_buf, 21);
    uint32_t ihdr_crc = png_crc32(ihdr_buf + 4, 17, 0xFFFFFFFF) ^ 0xFFFFFFFF;
    b64_write_u32(ihdr_crc);

    // IDAT chunk: 64 rows * (1 filter + 16 pixel bytes) = 1088 raw bytes
    // Zlib: header(2) + deflate stored(5) + data(1088) + adler32(4) = 1099
    static const uint16_t IDAT_LEN = 1099;
    static const uint16_t RAW_LEN  = 1088;

    uint8_t idat_len[4] = {
        (uint8_t)(IDAT_LEN >> 24), (uint8_t)(IDAT_LEN >> 16),
        (uint8_t)(IDAT_LEN >> 8),  (uint8_t)(IDAT_LEN)
    };
    b64_write(idat_len, 4);

    static const uint8_t idat_type[] = {'I', 'D', 'A', 'T'};
    b64_write(idat_type, 4);
    uint32_t idat_crc = png_crc32(idat_type, 4, 0xFFFFFFFF);

    // Zlib header
    uint8_t zlib_hdr[] = {0x78, 0x01};
    b64_write(zlib_hdr, 2);
    idat_crc = png_crc32(zlib_hdr, 2, idat_crc);

    // Deflate stored block: BFINAL=1, BTYPE=00
    uint8_t deflate_hdr[] = {
        0x01,
        (uint8_t)(RAW_LEN & 0xFF), (uint8_t)(RAW_LEN >> 8),
        (uint8_t)(~RAW_LEN & 0xFF), (uint8_t)((~RAW_LEN >> 8) & 0xFF)
    };
    b64_write(deflate_hdr, 5);
    idat_crc = png_crc32(deflate_hdr, 5, idat_crc);

    // Scanlines + Adler32
    uint32_t adler_a = 1, adler_b = 0;
    for (uint8_t y = 0; y < 64; y++) {
        uint8_t filter = 0x00;
        b64_write_byte(filter);
        idat_crc = png_crc32(&filter, 1, idat_crc);
        adler_a = (adler_a + filter) % 65521;
        adler_b = (adler_b + adler_a) % 65521;

        convert_page_row(fb, y, row_buf);
        b64_write(row_buf, 16);
        idat_crc = png_crc32(row_buf, 16, idat_crc);
        for (uint8_t i = 0; i < 16; i++) {
            adler_a = (adler_a + row_buf[i]) % 65521;
            adler_b = (adler_b + adler_a) % 65521;
        }
    }

    // Adler32 checksum
    uint32_t adler = (adler_b << 16) | adler_a;
    uint8_t adler_bytes[4] = {
        (uint8_t)(adler >> 24), (uint8_t)(adler >> 16),
        (uint8_t)(adler >> 8),  (uint8_t)(adler)
    };
    b64_write(adler_bytes, 4);
    idat_crc = png_crc32(adler_bytes, 4, idat_crc);

    idat_crc ^= 0xFFFFFFFF;
    b64_write_u32(idat_crc);

    // IEND chunk
    static const uint8_t PROGMEM iend[] = {
        0x00, 0x00, 0x00, 0x00,  // length = 0
        0x49, 0x45, 0x4E, 0x44,  // "IEND"
        0xAE, 0x42, 0x60, 0x82   // CRC (well-known)
    };
    uint8_t iend_buf[12];
    memcpy_P(iend_buf, iend, 12);
    b64_write(iend_buf, 12);

    b64_flush();
    Serial.println("--- PNG END ---");
}
