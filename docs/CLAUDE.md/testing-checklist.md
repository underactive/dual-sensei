# Testing Checklist

## Epoch 1: Scaffolding + Display + Input

- [ ] Power on shows splash screen "DUAL-SENSEI v0.1.0" for ~2 seconds, then transitions to visualizer
- [ ] Onboard LED (GPIO 2) lights during init, turns off when ready
- [ ] Visualizer screen shows PS1 controller layout with all buttons in released state
- [ ] Visualizer status line shows "No Controller" (left) and player number from NVS (right)
- [ ] Visualizer PS1 bytes line shows "PS1: FF FF" (all released)
- [ ] D-pad rendered as cross shape with outlined arms and filled center
- [ ] Face buttons (triangle, circle, cross, square) rendered as outlined circles with symbols
- [ ] Shoulder buttons (L1, L2, R1, R2) and Select/Start shown as text labels
- [ ] Pressing CON from visualizer opens Settings menu
- [ ] Encoder rotates through menu items (Trigger Thresh, Stick->DPad, Player Number, Pairing, About)
- [ ] Selecting "Trigger Thresh" and pressing CON enters edit mode (value shown in brackets)
- [ ] Rotating encoder in edit mode changes trigger threshold value (0-255)
- [ ] Pressing CON in edit mode saves value and returns to settings list
- [ ] Pressing BAK in edit mode discards change and returns to settings list
- [ ] Selecting "Stick->DPad" toggles between ON/OFF on encoder rotation
- [ ] Selecting "Player Number" toggles between P1/P2 on encoder rotation
- [ ] Selecting "Pairing..." shows pairing instructions screen
- [ ] Pressing BAK from pairing returns to settings
- [ ] Selecting "About..." shows firmware name, version, description
- [ ] Pressing BAK from about returns to settings
- [ ] Pressing BAK from settings returns to visualizer
- [ ] Changed settings persist after power cycle (NVS)
- [ ] Changing player number is reflected on visualizer status line after returning from menu
- [ ] No crashes or watchdog resets after 10 minutes of operation
- [ ] Serial monitor shows init messages: display, input, menu with loaded settings
