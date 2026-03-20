# Multi-Console Roadmap

Dual-Sensei's primary target is PS1/PS2, with expansion to other retro consoles ordered by implementation difficulty. The wireless controller input side (Bluepad32 BT → ControllerState) is console-agnostic — only the output protocol changes per console.

## Architecture

### Hardware

```
                          ┌─────────────────────┐
[BT Controller] ──BT──▶  │  ESP32 Core Board    │
                          │  (OLED + Encoder)    │
                          └──────┬──────────────┘
                                 │ GPIO header
                          ┌──────┴──────────────┐
                          │  Console Adapter     │ ◄── Swappable per target console
                          │  • Level shifter     │
                          │  • Console connector │
                          └─────────────────────┘
```

- **Core board**: ESP32-WROOM-32 + SH1106 OLED + EC11 encoder + buttons (unchanged)
- **Console adapter boards**: Per-console PCB with level shifter (if 5V) + connector. Could include an ID resistor for auto-detection.
- **3.3V consoles** (PS1/PS2, N64): No level shifter — direct wiring through adapter
- **5V consoles** (everything else): Bidirectional level shifter (TXS0108E, 74LVC245, or BSS138-based) on adapter

### Firmware

Extract `compute_protocol_bytes()` from `display.cpp` into a protocol abstraction layer:

```
main/
├── proto/
│   ├── proto.h           # Interface: proto_init(), proto_update(), proto_get_name()
│   ├── proto_psx.cpp     # PS1/PS2 SPI slave
│   ├── proto_gpio.cpp    # Atari 2600 / Neo Geo (direct pin toggle)
│   ├── proto_shift.cpp   # NES / SNES (shift register)
│   ├── proto_tg16.cpp    # TurboGrafx-16 (SEL mux)
│   ├── proto_genesis.cpp # Genesis 3-btn / 6-btn (SELECT mux + counter)
│   ├── proto_saturn.cpp  # Saturn (2-SEL mux)
│   └── proto_n64.cpp     # N64 Joybus (RMT peripheral)
```

Each module implements the same interface: takes a `ControllerState`, drives the appropriate GPIO signals for that console. The "Console Mode" NVS setting selects the active protocol. The visualizer's protocol bytes display adapts to show the active protocol's output format.

### Pin Sharing

PS1/PS2 uses VSPI (GPIO 5, 18, 19, 23, 4). Other consoles use different pin counts and configurations. Options:
- **Dedicated pins per console**: Wastes GPIOs but simplest (adapter only connects the pins it needs)
- **Shared GPIO header**: All protocol pins routed to a common header; adapter board wires them to the correct connector. Console Mode setting configures pin directions/modes at runtime.
- **Adapter ID pin**: Adapter board ties an ADC pin to a specific voltage via resistor divider; firmware auto-detects console type on boot.

Recommended: shared GPIO header with runtime pin configuration. The adapter handles level shifting and connector mapping.

---

## Console Tiers

### Primary — PS1 / PS2

| Property | Value |
|----------|-------|
| Protocol | SPI slave (CLK/CMD/DAT/ATT/ACK byte exchange) |
| Signal pins | 5 (CLK, CMD, DAT, ATT, ACK) |
| Voltage | 3.3V (no level shifter) |
| Timing | ~4μs per byte, 60Hz polling |
| ESP32 approach | VSPI peripheral in slave mode, ACK via manual GPIO pulse |
| Connector | PS1/PS2 extension cable (cut male end) |
| Status | Pin assignments in config.h, protocol constants defined, `compute_protocol_bytes()` implemented in display.cpp (needs extraction) |

**PS1 mode**: Digital pad ID `0x41`, 2 data bytes (buttons only).
**PS2 mode**: DualShock 2 ID `0x73`, 6 data bytes (buttons + 4 stick bytes).

**Prerequisites**: Move display rendering to a separate FreeRTOS task (Known Issue #3) so I2C blocking doesn't interfere with SPI response latency.

---

### Tier 1 — Direct GPIO (Trivial)

#### Atari 2600

| Property | Value |
|----------|-------|
| Protocol | Direct switches to ground (no timing, no protocol) |
| Signal pins | 5 (Up, Down, Left, Right, Fire) |
| Voltage | 5V — **level shifter required** |
| Timing | None — purely static GPIO levels |
| ESP32 approach | Open-drain GPIOs, pull LOW = pressed, high-Z = released |
| Connector | DE-9 female (same as serial port) |
| Button mapping | D-pad → directions, Cross → Fire |

#### Neo Geo

| Property | Value |
|----------|-------|
| Protocol | Direct switches to ground (same as Atari, more buttons) |
| Signal pins | 10 (Up, Down, Left, Right, A, B, C, D, Select, Start) |
| Voltage | 5V — **level shifter required** |
| Timing | None |
| ESP32 approach | Same as Atari — open-drain GPIOs |
| Connector | DA-15 female (15-pin D-sub). **WARNING**: Pin 8 is +5V output from console — do not short to ground. |
| Button mapping | D-pad → directions, Cross/Circle/Square/Triangle → A/B/C/D |

---

### Tier 2 — Shift Register (Easy)

#### NES

| Property | Value |
|----------|-------|
| Protocol | Synchronous serial shift register (8-bit, ~12μs/bit) |
| Signal pins | 3 (LATCH input, CLK input, DATA output) |
| Voltage | 5V — **level shifter required** |
| Timing | LATCH pulse 12μs, CLK 12μs period. Poll rate 60Hz. |
| ESP32 approach | ISR on LATCH rising edge snapshots button state, ISR on CLK rising edge shifts out next bit |
| Connector | 7-pin proprietary |
| Bit order (active LOW) | A, B, Select, Start, Up, Down, Left, Right |
| Button mapping | Cross → A, Circle → B, Select → Select, Start → Start |

#### SNES

| Property | Value |
|----------|-------|
| Protocol | Synchronous serial shift register (16-bit, same timing as NES) |
| Signal pins | 3 (LATCH input, CLK input, DATA output) |
| Voltage | 5V — **level shifter required** |
| Timing | Same as NES but 16 clock pulses instead of 8 |
| ESP32 approach | Same as NES, shift out 16 bits instead of 8 |
| Connector | 7-pin proprietary (different shape from NES) |
| Bit order (active LOW) | B, Y, Select, Start, Up, Down, Left, Right, A, X, L, R, (4 unused) |
| Button mapping | Cross → B, Circle → A, Square → Y, Triangle → X, L1 → L, R1 → R |

#### TurboGrafx-16 / PC Engine

| Property | Value |
|----------|-------|
| Protocol | Multiplexed parallel (1 SEL line, 4 data lines) |
| Signal pins | 6 (SEL input, CLR input, 4 data outputs) |
| Voltage | 5V — **level shifter required** |
| Timing | ~1.25μs settling after SEL change, 60Hz polling |
| ESP32 approach | ISR on SEL, present directional or button group on data pins |
| Connector | 8-pin Mini DIN (PCE) / 8-pin DIN (TG-16) |
| SEL=HIGH | Up, Right, Down, Left |
| SEL=LOW | Button I, Button II, Select, Run |
| Button mapping | Cross → I, Circle → II, Select → Select, Start → Run |

---

### Tier 3 — Multiplexed Parallel (Easy–Moderate)

#### Sega Saturn

| Property | Value |
|----------|-------|
| Protocol | Multiplexed parallel (2 SEL lines, 4 data lines) |
| Signal pins | 6 (S0 input, S1 input, D0–D3 outputs) |
| Voltage | 5V — **level shifter required** |
| Timing | Combinational (no clock) — respond as fast as outputs settle |
| ESP32 approach | ISR on S0/S1, present correct button group on D0–D3 |
| Connector | Custom 9-pin (not DE-9) |
| 4 mux states | S1:S0=00 → R,X,Y,Z; 01 → Start,A,C,B; 10 → Right,Left,Down,Up; 11 → L trigger |
| Button mapping | Cross → A, Circle → B, Square → X, Triangle → Y, L1 → L, R1 → R, L2 → Z, Start → Start |

#### Sega Genesis / Mega Drive

| Property | Value |
|----------|-------|
| Protocol | Multiplexed parallel (SELECT line toggles button groups) |
| Signal pins | 7 (SELECT input, 6 data outputs on pins 1–4, 6, 9) |
| Voltage | 5V — **level shifter required** |
| Timing | ~4.6μs per SELECT state, 1.5ms counter reset timeout (6-button) |
| ESP32 approach | ISR on SELECT edges. 3-button: 2 states. 6-button: internal counter tracks 8-state cycle, resets on 1.5ms timeout. |
| Connector | DE-9 male (same physical as Atari, different protocol) |
| 3-button | A, B, C, Start, Up, Down, Left, Right |
| 6-button (adds) | X, Y, Z, Mode |
| Button mapping | Cross → B, Circle → C, Square → A (3-btn) or Square → X, Triangle → Y, R1 → Z, L1 → Mode (6-btn) |

---

### Tier 4 — Tight Timing (Hard)

#### Nintendo 64

| Property | Value |
|----------|-------|
| Protocol | Single-wire half-duplex serial ("Joybus"), pulse-width encoded |
| Signal pins | 1 (DATA — open-drain with external 1kΩ pull-up to 3.3V) |
| Voltage | **3.3V native — no level shifter needed** |
| Timing | 4μs per bit (1μs LOW + 3μs HIGH = bit "1"; 3μs LOW + 1μs HIGH = bit "0"). Must respond ~2μs after console stop bit. |
| ESP32 approach | RMT peripheral for hardware-timed pulse generation, or bare-metal register access with interrupts disabled. Direct `GPIO_OUT_REG` writes, not `digitalWrite()`. |
| Connector | 3-pin proprietary (GND, Data, 3.3V) |
| Commands | 0x00 = Info (3-byte response), 0x01 = Button status (4-byte response), 0xFF = Reset |
| Button status | 32 bits: A, B, Z, Start, D-pad (4), reserved, L, R, C-buttons (4), analog X (signed 8-bit), analog Y (signed 8-bit) |
| Button mapping | Cross → A, Circle → B, Square → (C-Left?), Triangle → (C-Up?), L1 → L, R1 → R, L2 → Z, Select → (none), Start → Start, analog sticks → stick axes |
| Prerequisites | Display rendering MUST be in separate FreeRTOS task. Cannot tolerate any interrupt latency during Joybus communication. |

---

### Nice to Have — Very Hard

#### Sega Dreamcast

| Property | Value |
|----------|-------|
| Protocol | Maple Bus — 2-wire alternating clock/data, packet-based |
| Signal pins | 2 (SDCKA, SDCKB — both bidirectional) |
| Voltage | **3.3V signals, 5V power — no signal level shifter needed** |
| Timing | ~160–250ns per phase (~2 Mbps). Only ~40–60 CPU cycles at 240MHz. |
| ESP32 approach | **Likely not feasible with ESP32 alone.** Would need an RP2350 PIO co-processor or FPGA on the adapter board. |
| Connector | Proprietary |
| Recommendation | **Defer indefinitely** unless an RP2350 co-processor adapter is designed. |

---

## Implementation Order

| Phase | Target | Depends On |
|-------|--------|-----------|
| **Phase 2** | PS1/PS2 SPI slave | Display task separation (Known Issue #3) |
| **Phase 3a** | Protocol abstraction layer (`proto/` module) | Phase 2 complete |
| **Phase 3b** | Atari 2600 + Neo Geo (direct GPIO) | Phase 3a + level shifter adapter board |
| **Phase 3c** | NES + SNES (shift register) | Phase 3a + level shifter adapter board |
| **Phase 3d** | TurboGrafx-16 (mux parallel) | Phase 3a + level shifter adapter board |
| **Phase 3e** | Saturn + Genesis (mux parallel) | Phase 3a + level shifter adapter board |
| **Phase 4** | N64 (Joybus via RMT) | Phase 3a + display task separation verified under tight timing |
| **Deferred** | Dreamcast (Maple Bus) | Co-processor adapter design |

## Hardware Bill of Materials (per adapter)

| Component | Purpose | Consoles |
|-----------|---------|----------|
| TXS0108E or 74LVC245 | 3.3V ↔ 5V level shifting | All 5V consoles |
| 1kΩ pull-up resistor | Data line pull-up to 3.3V | N64 only |
| Console extension cable | Cut for male connector + wires | All |
| PCB or protoboard | Adapter board | All |
| ID resistor (optional) | Voltage divider on ADC pin for auto-detect | All (if auto-detect desired) |

## Open Questions

1. **Pin sharing vs dedicated pins**: Can all console protocols share the same GPIO header, or do some need dedicated pins (e.g., VSPI for PSX can't easily be reused for parallel output)?
2. **Console mode menu expansion**: The current "Console Mode" setting toggles PS1/PS2. Expand to a full console selector, or auto-detect from adapter board?
3. **Visualizer per console**: Should the visualizer adapt to show each console's native controller layout (NES pad, SNES pad, Genesis 3/6-button, etc.), or always show the connected BT controller's layout?
4. **Button mapping profiles**: Different consoles have different button counts and layouts. Should mappings be hardcoded or user-configurable via menu?
5. **Dual adapter support**: Could one ESP32 drive two console protocols simultaneously (e.g., 2-player with two adapters on the same board)?
