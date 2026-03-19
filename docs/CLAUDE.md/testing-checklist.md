# Testing Checklist

## Epoch 1: Scaffolding + Display + Input

- [ ] Power on shows splash screen "DUAL-SENSEI v0.1.0" for ~2 seconds, then transitions to visualizer
- [ ] Onboard LED (GPIO 2) lights during init, turns off when ready
- [ ] Visualizer screen shows "Input Test" with encoder position and button states
- [ ] Rotating encoder clockwise increments the encoder position counter
- [ ] Rotating encoder counter-clockwise decrements the encoder position counter
- [ ] Pressing CON button shows CON:1 on visualizer (while held)
- [ ] Pressing BAK button shows BAK:1 on visualizer (while held)
- [ ] Pressing encoder push (PHS) shows PHS:1 on visualizer (while held)
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
- [ ] No crashes or watchdog resets after 10 minutes of operation
- [ ] Serial monitor shows init messages: display, input, menu with loaded settings
