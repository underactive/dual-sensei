# Version History

| Ver | Changes |
|-----|---------|
| v0.2.0 | Epoch 2: Bluepad32 BT integration (pairing, button/trigger/stick mapping), multi-controller support (DualSense, DualShock 4, Xbox One, Switch Pro), PS2 scope expansion (console mode setting, analog sticks, L3/R3, mode-dependent visualizer), touchpad-to-Select/Start (DS4 + DualSense), rumble API + test rumble menu action, controller name on visualizer, multi-controller pairing screen, source migration from src/ to main/ (ESP-IDF convention), pioarduino platform with custom partitions and BTstack sdkconfig |
| v0.1.0 | Initial scaffolding: PlatformIO project, SH1106 OLED display (splash/pairing/visualizer/menu screens), ISR-driven rotary encoder + button input with FreeRTOS queue, menu state machine with NVS settings persistence (trigger threshold, stick-to-dpad, player number) |
