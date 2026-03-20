# Plan: PS1/PS2 SPI Slave Protocol

## Objective

Implement the PSX controller SPI slave protocol so the ESP32 responds to a real PS1 or PS2 console as a controller. This is the core bridge functionality — translating BT controller data (already flowing through `ControllerState`) into real-time SPI responses on the controller port.

### Scope

**In scope:**
- PS1 digital pad mode (ID `0x41`, 5-byte transaction)
- PS2 DualShock 2 analog mode (ID `0x73`, 9-byte transaction)
- Manual ACK pulse generation
- Display rendering moved to separate FreeRTOS task (prerequisite)
- Extract `compute_protocol_bytes()` from `display.cpp` into shared module

**Deferred (Phase 2b):**
- Config/escape mode commands (0x43, 0x44, 0x4D, 0x4F)
- Rumble motor forwarding (CMD bytes 3-4 → `bt_play_rumble()`)
- Pressure-sensitive button mode (ID `0x79`, 21 bytes)
- Multi-tap support

### Why register-level SPI, not ESP-IDF spi_slave

The PSX protocol is a byte-by-byte reactive exchange — each response byte depends on the previous command byte. ESP-IDF's `spi_slave_transmit()` API expects pre-loaded buffers and uses FreeRTOS task notifications, adding microseconds of latency that can miss the ~12μs inter-byte window. BlueRetro (the reference ESP32 PSX implementation) uses direct register access for this reason.

---

## PSX Protocol Summary

### SPI Configuration

| Parameter | Value |
|-----------|-------|
| Mode | **Mode 3** (CPOL=1 idle high, CPHA=1 sample on rising edge) |
| Bit order | **LSB first** |
| Clock | ~250 kHz (4μs/bit, 32μs/byte) |
| Full-duplex | Yes — CMD and DAT exchanged simultaneously |

### PS1 Digital Pad Transaction (ID 0x41)

| Byte | CMD (console) | DAT (controller) | ACK? |
|------|---------------|-------------------|------|
| 0 | 0x01 (address) | 0xFF (hi-Z) | Yes |
| 1 | 0x42 (poll) | 0x41 (device ID) | Yes |
| 2 | 0x00 | 0x5A (data ready) | Yes |
| 3 | 0x00 | buttons_lo | Yes |
| 4 | 0x00 | buttons_hi | **No** (last byte) |

### PS2 DualShock 2 Transaction (ID 0x73)

| Byte | CMD (console) | DAT (controller) | ACK? |
|------|---------------|-------------------|------|
| 0 | 0x01 | 0xFF | Yes |
| 1 | 0x42 | 0x73 | Yes |
| 2 | 0x00 | 0x5A | Yes |
| 3 | 0x00 | buttons_lo | Yes |
| 4 | 0x00 | buttons_hi | Yes |
| 5 | 0x00 | right_stick_x | Yes |
| 6 | 0x00 | right_stick_y | Yes |
| 7 | 0x00 | left_stick_x | Yes |
| 8 | 0x00 | left_stick_y | **No** (last byte) |

### ACK Timing

- Delay: ~12μs after last SCK rising edge (valid range 3–100μs)
- Pulse: pull ACK LOW for ≥2μs, then release (open-drain)
- No ACK after the final byte of the transaction

### Pin Mapping (already in config.h)

| PSX Pin | Signal | ESP32 GPIO | Mode |
|---------|--------|------------|------|
| 7 | CLK | 18 | SPI CLK input |
| 2 | CMD | 23 | SPI MOSI input |
| 1 | DAT | 19 | SPI MISO output (open-drain) |
| 6 | ATT | 5 | SPI CS input + GPIO interrupt |
| 9 | ACK | 4 | GPIO output (open-drain) |

---

## Changes

### Step 1: Move display rendering to FreeRTOS task

**Files:** `main/display.h`, `main/display.cpp`, `main/main.cpp`

Currently `display_update()` calls `u8g2.sendBuffer()` which blocks the main loop for ~20ms (I2C transfer of 1KB framebuffer). The SPI slave ISR must respond within microseconds. While ISRs preempt normal code, moving display to its own task provides cleaner separation and prevents any contention.

- Create a FreeRTOS task `display_task` (pinned to CPU1, priority 1 — below default Arduino loop priority)
- The task runs a loop: render the current screen into the framebuffer, call `sendBuffer()`, then `vTaskDelay()` for frame pacing
- `display_update()` becomes a no-op or signals the task (the task polls at its own rate)
- `display_set_controller()` writes to a shared `ControllerState` protected by a mutex or atomic copy (both writer and reader are on CPU1, so a simple volatile copy may suffice)
- `display_screenshot()` needs synchronization — either runs in the display task context or grabs the mutex before reading the buffer

### Step 2: Extract protocol byte computation

**Files:** `main/psx.h` (new), `main/psx.cpp` (new), `main/display.cpp` (modified)

Move `compute_protocol_bytes()` from `display.cpp` to a new `psx` module. Both the display (for visualizer protocol bytes) and the SPI ISR (for real responses) need this function.

- `psx.h`: Declare `psx_build_response()` — takes `ControllerState` + console mode, fills a response buffer with the full transaction (ID, 0x5A, button bytes, stick bytes)
- `psx.cpp`: Implementation, extracted from existing `compute_protocol_bytes()` logic
- `display.cpp`: Calls `psx_build_response()` instead of its local `compute_protocol_bytes()`
- Add the new module to `main/CMakeLists.txt`

### Step 3: SPI slave driver (register-level)

**Files:** `main/psx_spi.h` (new), `main/psx_spi.cpp` (new), `main/config.h` (additions)

Implement the SPI slave using direct register access on VSPI (SPI3):

#### Initialization (`psx_spi_init()`)
1. Configure GPIO pins:
   - CLK (18): input, SPI function
   - CMD (23): input, SPI function
   - DAT (19): output, open-drain, SPI function
   - ATT (5): input with GPIO interrupt on both edges
   - ACK (4): output, open-drain, default high-Z
2. Configure VSPI peripheral in slave mode:
   - SPI Mode 3 (CPOL=1, CPHA=1) — note: ESP32 SPI register-level config may need inverted edge settings per BlueRetro's findings
   - LSB first
   - 8-bit transfers
   - Enable SPI interrupt (trans_done)
3. Pin the SPI ISR to CPU1
4. Pre-load first response byte (0xFF) for when ATT falls

#### ISR — byte complete (`psx_spi_isr()`)

State machine driven by byte index within the current transaction:

```
byte 0: Read CMD. If 0x01 (controller address), load ID byte (0x41 or 0x73).
         If not 0x01 (memory card or unknown), ignore rest of transaction.
         Send ACK.
byte 1: Read CMD. Should be 0x42 (poll). Load 0x5A (data ready marker).
         Send ACK.
byte 2: Read CMD (ignored). Load buttons_lo from response buffer.
         Send ACK.
byte 3+: Load next response byte from pre-built buffer.
          Send ACK unless this is the last byte.
```

After each byte (except last):
1. Load next DAT byte into SPI TX register
2. Re-arm SPI transaction (`spi_hw->cmd.usr = 1`)
3. Wait ~12μs (`ets_delay_us(12)`)
4. Pull ACK LOW for ~2μs, then release

#### ATT interrupt handler
- **Falling edge**: Reset byte counter to 0, snapshot current `ControllerState` into response buffer via `psx_build_response()`, pre-load first byte (0xFF)
- **Rising edge**: Reset state machine, ensure SPI is ready for next transaction

#### Response buffer update (`psx_spi_set_state()`)
- Called from the main loop after `bt_update()` — copies `ControllerState` into a buffer that the ATT ISR snapshots on each transaction start
- Must be fast (struct copy) and safe (disable interrupts briefly or use double-buffering)

### Step 4: Integration in main loop

**Files:** `main/main.cpp`

```cpp
void setup() {
    // ... existing init ...
    psx_spi_init();
}

void loop() {
    InputEvent evt = input_poll();
    if (evt != INPUT_NONE) menu_handle_input(evt);

    bt_update();

    // Update SPI response buffer with latest controller state
    psx_spi_set_state(bt_get_state(), menu_get_console_mode());

    // Push to display task (if visualizer active)
    if (display_get_screen() == SCREEN_VISUALIZER) {
        display_set_controller(bt_get_state());
    }

    // Serial commands
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 's') display_screenshot();
    }

    vTaskDelay(1);
}
```

### Step 5: Config constants

**Files:** `main/config.h`

Add SPI timing constants:
```c
#define PSX_ACK_DELAY_US     12    // Delay after last SCK before ACK pulse
#define PSX_ACK_PULSE_US      2    // ACK LOW duration
#define PSX_SPI_HOST       SPI3_HOST  // VSPI
```

---

## Dependencies

1. **Step 1** (display task) must complete before Step 3 (SPI slave) to avoid I2C blocking interfering with SPI timing validation
2. **Step 2** (extract protocol bytes) must complete before Step 3 (SPI ISR calls `psx_build_response()`)
3. **Step 3** and Step 4 can be developed together
4. **Hardware wiring** must be done before testing Step 3 against real console

## Risks / Open Questions

### High risk

1. **ESP32 SPI register-level configuration for Mode 3 + LSB-first**: BlueRetro's source shows that the ESP32's SPI peripheral requires non-obvious register settings to match PSX timing. The `clk_idle_edge` and `clk_i_edge` bits may need to be set opposite to what SPI Mode 3 suggests. Must verify against a logic analyzer or real console.

2. **Open-drain MISO (DAT)**: The ESP32 SPI peripheral normally drives MISO push-pull. For PSX protocol, DAT must be open-drain (console has pull-ups). May need to configure the GPIO matrix to route SPI output through open-drain mode. BlueRetro handles this — need to study their GPIO matrix setup.

3. **ISR latency budget**: The ISR must read the CMD byte, compute the response, load the TX register, and send ACK — all within ~48μs (one byte period at 250 kHz). The `ets_delay_us(12)` for ACK timing consumes 12μs of that budget. If the ISR entry latency + computation exceeds ~36μs, we'll miss the window. Should be fine (ESP32 ISR entry is ~2μs, register reads are ~100ns) but must verify.

### Medium risk

4. **GPIO 5 (ATT/CS) boot behavior**: GPIO 5 is the VSPI CS pin but also affects boot mode (must be HIGH during boot). The PSX console may hold ATT in an unknown state during ESP32 power-up. May need a pull-up resistor on GPIO 5, or delay SPI init until after boot.

5. **VSPI conflict with boot pins**: GPIO 5 has a default pull-up at boot. Should be safe, but verify the PSX console doesn't drive it LOW during ESP32 boot.

6. **Byte 0 response timing**: The first byte (0xFF) must be ready before the console clocks it in. The ATT falling edge ISR must pre-load the TX register before the first CLK edge arrives. The delay between ATT falling and first CLK is console-dependent (likely a few μs) — should be enough for an ISR to fire and load the register.

### Low risk

7. **Display task stack size**: U8g2 full-buffer mode uses ~1KB on the stack for the framebuffer. The display task needs sufficient stack allocation (4096 bytes should be plenty).

8. **Console mode switching**: If the user changes Console Mode (PS1↔PS2) in the menu while the SPI slave is active, the response buffer format changes between 5 and 9 bytes. The ATT ISR snapshots the mode at transaction start, so mid-transaction switching is safe. The console may see one "wrong" response during the transition frame — acceptable.

## Verification Plan

### Without real console (bench testing)
- Serial log confirms SPI init, pin configuration, ISR registration
- Use a second ESP32 (or logic analyzer) as SPI master to simulate PSX polling
- Verify byte-by-byte responses match expected protocol
- Verify ACK pulse timing with oscilloscope or logic analyzer

### With real PS1
- Wire adapter cable to ESP32 VSPI pins per `docs/wiring.md`
- Boot PS1 with no disc (goes to memory card/CD menu)
- PS1 should detect controller (menu shows controller icon)
- Navigate PS1 menu with D-pad + Cross/Circle
- Verify all button mappings

### With real PS2
- Same wiring (PS1/PS2 use same connector)
- Boot PS2, navigate to browser menu
- Verify analog sticks work (move cursor)
- Verify L3/R3, L2/R2 triggers
- Test with a game that requires analog input

## New Files

| File | Purpose |
|------|---------|
| `main/psx.h` | Protocol byte computation (extracted from display.cpp) |
| `main/psx.cpp` | Protocol byte computation implementation |
| `main/psx_spi.h` | SPI slave driver public API |
| `main/psx_spi.cpp` | Register-level SPI slave, ISR, ACK generation |

## Modified Files

| File | Change |
|------|--------|
| `main/display.h` | Remove or keep `display_update()` signature, add task-related API |
| `main/display.cpp` | Move rendering loop to FreeRTOS task, replace `compute_protocol_bytes()` with `psx_build_response()` call |
| `main/main.cpp` | Add `psx_spi_init()`, `psx_spi_set_state()` calls, remove `display_update()` from loop |
| `main/config.h` | Add SPI timing constants |
| `main/CMakeLists.txt` | Add `psx.cpp`, `psx_spi.cpp` to source list |
| `CLAUDE.md` | Update architecture, add PSX SPI subsystem docs |
