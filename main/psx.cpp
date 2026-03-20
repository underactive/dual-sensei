#include "psx.h"
#include "config.h"

uint8_t psx_build_response(const ControllerState& cs, uint8_t console_mode,
                           uint8_t* resp) {
    // Byte 0: Device ID
    resp[0] = (console_mode == 1) ? PS2_DEVICE_ID_ANALOG : PS1_DEVICE_ID;

    // Byte 1: Data ready marker
    resp[1] = PSX_DATA_MARKER;

    // Byte 2: buttons_lo — LT DN RT UP STRT [R3] [L3] SEL (active-low)
    // PS1: bits 1,2 always 1 (unused per digital pad spec). PS2: L3=bit1, R3=bit2.
    uint8_t lo = PSX_IDLE_BYTE;
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

    // Byte 3: buttons_hi — SQ X CIR TRI R1 L1 R2 L2 (active-low)
    uint8_t hi = PSX_IDLE_BYTE;
    if (cs.l2)       hi &= ~(1 << 0);
    if (cs.r2)       hi &= ~(1 << 1);
    if (cs.l1)       hi &= ~(1 << 2);
    if (cs.r1)       hi &= ~(1 << 3);
    if (cs.triangle) hi &= ~(1 << 4);
    if (cs.circle)   hi &= ~(1 << 5);
    if (cs.cross)    hi &= ~(1 << 6);
    if (cs.square)   hi &= ~(1 << 7);

    resp[2] = lo;
    resp[3] = hi;

    if (console_mode == 1) {  // PS2: append 4 stick bytes (RX, RY, LX, LY)
        resp[4] = cs.rx;
        resp[5] = cs.ry;
        resp[6] = cs.lx;
        resp[7] = cs.ly;
        return 8;
    }

    return 4;  // PS1: ID + 0x5A + 2 button bytes
}
