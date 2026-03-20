# pio_patches.py — Fix pioarduino platform warnings for ESP-IDF v5.4
#
# esp_idf_size --ng: The platform's firmware_metrics() post-action passes
# --ng to esp_idf_size, but ESP-IDF v5.4's version no longer accepts it
# (the "ng" mode is now the default). We monkey-patch subprocess.run to
# strip --ng from esp_idf_size invocations.
#
# esptool.py deprecation: The platform invokes esptool via the deprecated
# esptool.py entry point, which emits a log.warning(). This is an upstream
# pioarduino issue — the builder hardcodes the esptool.py path in OBJCOPY.
# Cannot be fixed from project config.

Import("env")

import subprocess

# ── Strip --ng from esp_idf_size subprocess calls ────────────────────
# The platform builder registers firmware_metrics() as a post-action on
# the ELF target. It calls: subprocess.run(["python", "-m", "esp_idf_size", "--ng", ...])
# Patch subprocess.run to drop --ng when the target is esp_idf_size.

_original_subprocess_run = subprocess.run

def _patched_subprocess_run(cmd, *args, **kwargs):
    if isinstance(cmd, list) and "-m" in cmd and "esp_idf_size" in cmd and "--ng" in cmd:
        cmd = [a for a in cmd if a != "--ng"]
    return _original_subprocess_run(cmd, *args, **kwargs)

subprocess.run = _patched_subprocess_run
