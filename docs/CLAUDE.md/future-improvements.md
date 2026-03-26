# Future Improvements (Ideas)

- [ ] OLED screen saver / auto-dim after idle timeout
- [ ] Configurable encoder sensitivity (steps-per-detent as NVS setting)
- [ ] Display contrast as a menu setting
- [ ] Boot animation or progress bar during init
- [ ] Analog stick deadzone as a configurable setting
- [ ] Button remapping (custom PS5→PS1 mapping profiles)
- [ ] DualSense touchpad: finer-grained zone mapping beyond current Select/Start halves
- [ ] DualSense LED color customization via menu
- [ ] OTA firmware updates via WiFi
- [ ] Battery monitoring if powered from LiPo in future enclosure
- [ ] Fix DualSense BT disconnect: L2CAP interrupt channel closes ~2s after setup — likely DualSense firmware (Jul 2025, v0x110002a) expects output report within tight window that Bluepad32 v4.2.0 doesn't provide. Try newer Bluepad32, or send initial output report from DualSense parser immediately after HID interrupt channel opens.
- [ ] Auto-enable PSX SPI when console ATT polling is detected (currently requires manual serial 'p' command)
- [ ] PSX SPI: revisit ESP32 SPI peripheral approach — bit-bang works but uses ~220μs of ISR time per transaction; SPI peripheral would free CPU1 if register issues can be resolved
