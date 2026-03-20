#include "bt.h"
#include "config.h"
#include "menu.h"

#include <Arduino.h>
#include <Bluepad32.h>

// ── State ──────────────────────────────────────────────────────────

static ControllerPtr connected_gamepad = nullptr;
static ControllerPtr connected_mouse   = nullptr;  // Virtual device (touchpad, DS4/DualSense)
static ControllerState ctrl_state;
static char controller_name[16] = "";

// Stick-to-DPad deadzone: ~25% of axis range (±512)
static const int32_t STICK_DEADZONE = 128;

// Convert Bluepad32 axis (-511..512) to PS2 range (0-255, 128=center)
// Uses / 4 instead of >> 2 to avoid implementation-defined signed right-shift
static uint8_t axis_to_ps2(int32_t val) {
    int32_t shifted = (val + 512) / 4;
    if (shifted > 255) shifted = 255;
    if (shifted < 0) shifted = 0;
    return (uint8_t)shifted;
}

// ── Callbacks ──────────────────────────────────────────────────────
// These run on CPU1 inside BP32.update(), same task as loop().

static void on_connected(ControllerPtr ctl) {
    // Check mouse first: virtual touchpad device (DS4/DualSense) has klass set immediately.
    // Don't gate on isGamepad() here — klass is set by init_report() on the first HID
    // report, which may arrive after this callback (observed with Xbox controllers).
    // Treat any non-mouse connection as a gamepad; map_controller_data() guards on
    // isGamepad() before processing actual input data.
    if (ctl->isMouse()) {
        connected_mouse = ctl;
        Serial.println("[bt] touchpad virtual device connected");
    } else {
        if (connected_gamepad != nullptr) {
            Serial.println("[bt] already have a gamepad, rejecting");
            ctl->disconnect();
            return;
        }
        connected_gamepad = ctl;
        // Store display-friendly name (e.g. "DualSense", "DualShock 4", "XBox One", "Switch Pro")
        // Properties (type, VID/PID) are available from SDP before first HID report.
        String model = ctl->getModelName();
        const char* cstr = model.c_str();
        // Shorten "XBox One" to "XBox" for display
        if (cstr && strcmp(cstr, "XBox One") == 0)
            cstr = "XBox";
        snprintf(controller_name, sizeof(controller_name), "%s", cstr ? cstr : "Controller");
        Serial.printf("[bt] gamepad connected: %s VID=0x%04x PID=0x%04x\n",
                      controller_name,
                      ctl->getProperties().vendor_id,
                      ctl->getProperties().product_id);
    }
}

static void on_disconnected(ControllerPtr ctl) {
    if (ctl == connected_gamepad) {
        connected_gamepad = nullptr;
        connected_mouse = nullptr;  // Virtual device goes with the gamepad
        // Reset controller state on disconnect (Development Rule #3)
        ctrl_state = ControllerState();
        controller_name[0] = '\0';
        Serial.println("[bt] gamepad disconnected");
    } else if (ctl == connected_mouse) {
        connected_mouse = nullptr;
        Serial.println("[bt] touchpad virtual device disconnected");
    }
}

// ── Mapping ────────────────────────────────────────────────────────

static void map_controller_data(ControllerPtr ctl) {
    if (!ctl || !ctl->isConnected() || !ctl->hasData() || !ctl->isGamepad())
        return;

    ctrl_state.connected = true;

    // D-pad
    uint8_t dpad = ctl->dpad();
    ctrl_state.up    = dpad & DPAD_UP;
    ctrl_state.down  = dpad & DPAD_DOWN;
    ctrl_state.left  = dpad & DPAD_LEFT;
    ctrl_state.right = dpad & DPAD_RIGHT;

    // Face buttons (Bluepad32 uses Xbox naming: A=south, B=east, X=west, Y=north)
    ctrl_state.cross    = ctl->a();      // DualSense Cross (south)
    ctrl_state.circle   = ctl->b();      // DualSense Circle (east)
    ctrl_state.square   = ctl->x();      // DualSense Square (west)
    ctrl_state.triangle = ctl->y();      // DualSense Triangle (north)

    // Shoulder buttons
    ctrl_state.l1 = ctl->l1();
    ctrl_state.r1 = ctl->r1();

    // Triggers: analog threshold OR digital button state.
    // DualSense/Xbox: brake()/throttle() give 0-1023 analog values, thresholded here.
    // Switch Pro: ZL/ZR are digital-only, report via l2()/r2() button bits (brake=0).
    // OR covers both: analog controllers use the threshold, digital controllers use the button.
    uint8_t thresh = menu_get_trigger_threshold();
    ctrl_state.l2 = ctl->l2() || (ctl->brake() / 4) >= thresh;
    ctrl_state.r2 = ctl->r2() || (ctl->throttle() / 4) >= thresh;

    // Stick presses (L3/R3)
    ctrl_state.l3 = ctl->thumbL();
    ctrl_state.r3 = ctl->thumbR();

    // Analog sticks → PS2 range (0-255, 128=center)
    ctrl_state.lx = axis_to_ps2(ctl->axisX());
    ctrl_state.ly = axis_to_ps2(ctl->axisY());
    ctrl_state.rx = axis_to_ps2(ctl->axisRX());
    ctrl_state.ry = axis_to_ps2(ctl->axisRY());

    // Select / Start (DualSense Create / Options)
    ctrl_state.select = ctl->miscSelect();
    ctrl_state.start  = ctl->miscStart();

    // Touchpad → Select/Start mapping (via virtual mouse device)
    // Bluepad32 maps left touchpad area → mouse BUTTON_A, right → BUTTON_B
    if (menu_get_touchpad_select() && connected_mouse &&
        connected_mouse->isConnected() && connected_mouse->hasData()) {
        if (connected_mouse->buttons() & BUTTON_A) ctrl_state.select = true;
        if (connected_mouse->buttons() & BUTTON_B) ctrl_state.start  = true;
    }

    // Stick-to-DPad mapping (OR'd with hardware D-pad)
    if (menu_get_stick_to_dpad()) {
        int32_t lx = ctl->axisX();   // -511 to 512
        int32_t ly = ctl->axisY();   // -511 to 512
        if (ly < -STICK_DEADZONE) ctrl_state.up    = true;
        if (ly >  STICK_DEADZONE) ctrl_state.down  = true;
        if (lx < -STICK_DEADZONE) ctrl_state.left  = true;
        if (lx >  STICK_DEADZONE) ctrl_state.right = true;
    }
}

// ── Public API ─────────────────────────────────────────────────────

void bt_init() {
    BP32.setup(&on_connected, &on_disconnected);
    // Enable virtual device so touchpad appears as a mouse with left/right buttons
    BP32.enableVirtualDevice(true);
    Serial.printf("[bt] Bluepad32 v%s initialized\n", BP32.firmwareVersion());
}

void bt_update() {
    bool updated = BP32.update();
    if (updated && connected_gamepad) {
        map_controller_data(connected_gamepad);
    }
}

bool bt_is_connected() {
    return connected_gamepad != nullptr && connected_gamepad->isConnected();
}

const ControllerState& bt_get_state() {
    return ctrl_state;
}

const char* bt_get_controller_name() {
    return controller_name;
}

void bt_start_pairing() {
    BP32.enableNewBluetoothConnections(true);
    Serial.println("[bt] scanning for controllers...");
}

void bt_play_rumble(uint16_t duration_ms, uint8_t weak, uint8_t strong) {
    if (connected_gamepad && connected_gamepad->isConnected()) {
        connected_gamepad->playDualRumble(0, duration_ms, weak, strong);
        Serial.printf("[bt] rumble: %ums weak=%u strong=%u\n", duration_ms, weak, strong);
    }
}

void bt_stop_pairing() {
    BP32.enableNewBluetoothConnections(false);
    Serial.println("[bt] scanning stopped");
}
