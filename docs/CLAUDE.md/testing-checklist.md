# Testing Checklist

## Epoch 1: Scaffolding + Display + Input

- [ ] Power on shows splash screen "DUAL-SENSEI v0.2.0" for ~2 seconds, then transitions to visualizer
- [ ] Onboard LED (GPIO 2) lights during init, turns off when ready
- [ ] Visualizer screen shows PS1 controller layout with all buttons in released state
- [ ] Visualizer status line shows "No Controller" (left) and player number from NVS (right)
- [ ] Visualizer PS1 bytes line shows "PS1: FF FF" (all released)
- [ ] D-pad rendered as cross shape with outlined arms and filled center
- [ ] Face buttons (triangle, circle, cross, square) rendered as outlined circles with symbols
- [ ] Shoulder buttons (L1, L2, R1, R2) and Select/Start shown as text labels
- [ ] Pressing CON or encoder push from visualizer opens Settings menu with first item selected (not pre-scrolled)
- [ ] Rapid encoder rotation before pressing CON does not cause settings to appear pre-scrolled (queue flush)
- [ ] Settings screen shows "Settings" header at top, separator line, help bar at bottom
- [ ] Two group headings visible: "- Controller -" and "- Device -" (centered, non-selectable)
- [ ] Encoder rotation skips heading rows and only stops on value/action items
- [ ] Selected item shows full-row inversion (white background, black text)
- [ ] Help bar updates to show item-specific help text when cursor moves
- [ ] Pressing CON on "Trigger Thresh" enters inline edit mode (value portion inverted with < > arrows)
- [ ] Help bar shows "Turn to adjust" while in edit mode
- [ ] `<` arrow hidden when trigger threshold is at 0 (minimum)
- [ ] `>` arrow hidden when trigger threshold is at 255 (maximum)
- [ ] Rotating encoder in edit mode changes trigger threshold value (0-255)
- [ ] Pressing CON or encoder push in edit mode saves value and returns to settings list
- [ ] Pressing BAK in edit mode discards change and returns to settings list
- [ ] "Stick to DPad" toggles between ON/OFF on encoder rotation (both arrows visible, wrapping)
- [ ] "Player Number" toggles between P1/P2 on encoder rotation
- [ ] "Pairing" action item shows `>` caret on right side, full-row inversion when selected
- [ ] Selecting "Pairing" shows pairing instructions screen
- [ ] Pressing BAK from pairing returns to settings
- [ ] "About" action item shows `>` caret on right side
- [ ] Selecting "About" shows firmware name, version, description
- [ ] Pressing BAK from about returns to settings
- [ ] Pressing BAK from settings returns to visualizer
- [ ] Changed settings persist after power cycle (NVS)
- [ ] Changing player number is reflected on visualizer status line after returning from menu
- [ ] Viewport scrolls correctly when navigating to items beyond the 5 visible rows
- [ ] Scrolling up to a group's first item also shows its preceding heading
- [ ] No crashes or watchdog resets after 10 minutes of operation
- [ ] Serial monitor shows init messages: display, input, menu, bt with loaded settings

## Epoch 2: DualSense Bluetooth

- [ ] Serial log shows "[bt] Bluepad32 v{version} initialized" on boot
- [ ] Put DualSense in pairing mode (hold Create + PS until light bar blinks)
- [ ] Navigate to Settings > Pairing and press CON — serial shows "[bt] scanning for controllers..."
- [ ] DualSense connects — serial shows "[bt] connected: DualSense VID=... PID=..."
- [ ] Visualizer status line changes from "No Controller" to "Connected"
- [ ] D-pad presses on DualSense are reflected on visualizer D-pad in real-time
- [ ] Face buttons (Cross, Circle, Square, Triangle) are reflected correctly
- [ ] L1/R1 shoulder buttons are reflected
- [ ] L2/R2 triggers activate when analog value exceeds trigger threshold setting
- [ ] Select (Create) and Start (Options) buttons are reflected
- [ ] PS1 protocol bytes at bottom of visualizer update with button presses (not all FF)
- [ ] Changing "Trigger Thresh" setting in menu affects L2/R2 activation sensitivity
- [ ] Enabling "Stick to DPad" causes left stick to activate D-pad directions on visualizer
- [ ] Stick-to-DPad OR's with physical D-pad (both work simultaneously)
- [ ] Pressing BAK from pairing screen shows "[bt] scanning stopped" in serial
- [ ] DualSense reconnects automatically after power cycle (BT keys stored in NVS)
- [ ] Disconnecting DualSense (turning it off) resets visualizer to "No Controller" and all buttons released
- [ ] Second DualSense attempting to connect is rejected (serial shows "[bt] already have a controller")
- [ ] Serial shows "[bt] touchpad virtual device connected" when DualSense pairs
- [ ] Rumble test works correctly after disconnecting and reconnecting the controller
- [ ] Pairing screen transitions from "Waiting for controller..." to "Controller Connected!" when controller pairs while on pairing screen

## Epoch 2: PS2 Scope Expansion

- [ ] PS1 mode: visualizer shows original layout (no sticks, no L3/R3, face buttons r=5)
- [ ] PS2 mode: L3/R3 text labels appear in shoulder row next to L1/R1
- [ ] PS2 mode: pressing L3 highlights L3 label (inverted text), pressing R3 highlights R3 label
- [ ] PS2 mode: analog stick circles appear between D-pad/Select and Start/face buttons
- [ ] PS2 mode: left stick position dot tracks DualSense left stick movement in real-time
- [ ] PS2 mode: right stick position dot tracks DualSense right stick movement in real-time
- [ ] Switching Console Mode between PS1/PS2 changes visualizer layout immediately
- [ ] "Console Mode" setting appears in Settings > Controller group, toggles between PS1/PS2
- [ ] Console Mode = PS1: protocol bytes line shows "PS1: XX XX" (2 bytes)
- [ ] Console Mode = PS2: protocol bytes line shows "PS2:XXYY XXYY XXYY" (6 bytes: buttons + sticks)
- [ ] Console Mode = PS2: L3/R3 presses reflected in protocol button bytes (bits 1,2 of low byte)
- [ ] Console Mode = PS2: stick movement reflected in protocol stick bytes (last 4 bytes change)
- [ ] Console Mode setting persists across power cycle (NVS key "con_mode")
- [ ] "Touchpad Sel/St" setting toggles between ON/OFF
- [ ] Touchpad Sel/St = ON: left touchpad area activates Select, right activates Start
- [ ] Player Number help text reads "Console port: P1 or P2"
- [ ] About screen shows "PS5-to-PSX Bridge"
- [ ] Serial log on boot shows console mode: "[menu] loaded — ... mode=PS1" or "mode=PS2"
- [ ] All existing Epoch 2 button mappings still work correctly after PS2 expansion
- [ ] "Test Rumble" action in Settings > Device sends a rumble pulse to connected controller
- [ ] "Test Rumble" with no controller connected does nothing (no crash)
- [ ] Serial log shows "[bt] rumble: 300ms weak=128 strong=200" on test rumble

## Multi-Controller Support

- [ ] Pairing screen shows 3 lines of instructions: DS4/5, Xbox, Switch
- [ ] Pairing help text reads "Pair wireless controller" (not DualSense-specific)
- [ ] DualShock 4 pairs successfully — serial shows "[bt] gamepad connected: DualShock 4 VID=... PID=..."
- [ ] DS4 visualizer status line shows "DualShock 4" (not "Connected")
- [ ] DS4 all buttons, triggers, sticks, D-pad reflected correctly on visualizer
- [ ] DS4 touchpad works with "Touchpad Sel/St" enabled (left=Select, right=Start)
- [ ] DS4 rumble test sends haptic feedback
- [ ] Xbox One controller pairs successfully — serial shows "[bt] gamepad connected: XBox One VID=... PID=..."
- [ ] Xbox visualizer status line shows "XBox One"
- [ ] Xbox all buttons, triggers, sticks, D-pad reflected correctly on visualizer
- [ ] Xbox rumble test sends haptic feedback
- [ ] Xbox "Touchpad Sel/St" has no effect (no crash)
- [ ] Switch Pro controller pairs successfully — serial shows "[bt] gamepad connected: Switch Pro VID=... PID=..."
- [ ] Switch Pro visualizer status line shows "Switch Pro"
- [ ] Switch Pro all buttons, sticks, D-pad reflected correctly on visualizer
- [ ] Switch Pro rumble test sends haptic feedback
- [ ] Switch Pro "Touchpad Sel/St" has no effect (no crash)
- [ ] Disconnecting any controller type resets visualizer to "No Controller" and all buttons released
- [ ] After disconnecting one controller type, a different type can pair successfully
- [ ] Controller name clears on disconnect, shows correct name on reconnect
