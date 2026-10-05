# CLAUDE.md

Live AQI monitor: classic ESP32 (not S3: DMD32Plus needs VSPI) + Sharp GP2Y1010AU0F dust sensor → ThingsBoard (MQTT, device access token) → dashboard, plus a P10 LED panel (DMD32Plus) scrolling the AQI.

- Firmware: Arduino sketch in `AQI_Monitor/` (folder name must match the .ino). Opens in Arduino IDE; PlatformIO builds it too via `src_dir = AQI_Monitor` (env `esp32dev`, `pio run` or `python -m platformio run`).
- `AQI_Monitor/secrets.h` is git-ignored; copy from `AQI_Monitor/secrets.example.h`. Never commit real credentials.
- Pins/timing/calibration constants live in `AQI_Monitor/config.h`. Sensor Vo must stay on an ADC1 pin, GPIO32-39 (ADC2 is unusable with Wi-Fi). Keep GPIO5 (SS) free: DMD32Plus skips refreshes while it reads LOW.
- Telemetry keys (`pm25`, `pm25Avg`, `aqi`, `aqiCategory`, `dustVoltage`, `dustZeroVoltage`, `samples`, `rssi`) are shared by `AQI_Monitor/AQI_Monitor.ino`, `tools/simulate.py`, and `docs/dashboard.md` — keep them in sync.
- AQI uses US EPA 2024 PM2.5 breakpoints by default (`AQI_STANDARD` in `config.h`, 2 = India CPCB); `AQI_Monitor/aqi.cpp` and `tools/simulate.py` duplicate the EPA table.
- Core 1 tasks: dust sampling (prio 3) > DMD refresh (prio 2, `vTaskDelay(1)`) > `loop()`. Never busy-wait on core 0 (Wi-Fi) and keep `loop()` non-blocking so the marquee keeps moving.
- Flashing and serial monitoring need the physical board, so they only work on the user's machine.
