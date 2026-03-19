# Wiring Guide

## Display Board Header (9-pin)

The display board has a 9-pin header. Wire to ESP32 as follows:

```
Header Pin:  CON  SDA  SCL  PHS  TRA  TRB  BAK  GND  VCC
ESP32 GPIO:  G26  G21  G22  G25  G32  G33  G27  GND  3V3
Wire Color:  YEL  BLU  GRN  ORG  WHT  GRY  PUR  BLK  RED
```

All button/encoder pins use ESP32 internal pull-ups (configured in firmware).
Buttons are active-low (press = LOW).

## Debug LEDs (Phase 1)

```
GPIO 2  ──[220Ω]── LED ── GND   (onboard LED, BT status)
GPIO 16 ──[220Ω]── LED ── GND   (test LED 1)
GPIO 17 ──[220Ω]── LED ── GND   (test LED 2)
```

LED current: ~10mA at 3.3V with 220Ω (safe for ESP32 GPIO).

## PS1 Controller Port (Phase 2)

Cut a PS1/PS2 extension cable. Wire the male plug end to ESP32:

```
PS1 Pin 1 (DAT) ── [33Ω] ── GPIO 19 (MISO, open-drain) + TVS diode to GND
PS1 Pin 2 (CMD) ── [33Ω] ── GPIO 23 (MOSI)
PS1 Pin 3 (7.6V)   N/C (leave disconnected)
PS1 Pin 4 (GND) ────────── ESP32 GND
PS1 Pin 5 (3.6V)   N/C (USB powers ESP32, not PS1 port)
PS1 Pin 6 (ATT) ── [33Ω] ── GPIO 5  (CS)
PS1 Pin 7 (CLK) ── [33Ω] ── GPIO 18 (CLK)
PS1 Pin 8 (N/C)    N/C
PS1 Pin 9 (ACK) ── [33Ω] ── GPIO 4  (manual GPIO, open-drain)
```

No level shifting needed: ESP32 3.3V ↔ PS1 3.6V is within mutual tolerance.
DAT and ACK are open-drain; PS1 provides pull-ups on those lines.

## Power

ESP32 powered via USB (500mA budget). PS1 port power pins are NOT connected.

| Component | Typical (mA) | Peak (mA) |
|-----------|-------------|-----------|
| ESP32-WROOM (active BT) | 95 | 240 |
| SH1106 OLED 1.3" | 18 | 30 |
| Encoder + buttons | <1 | <1 |
| Debug LEDs (3x) | 30 | 30 |
| **Total** | **~145** | **~300** |
