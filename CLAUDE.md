# CLAUDE.md

Live AQI monitor: ESP32-S3 + Sharp GP2Y1010AU0F dust sensor → ThingsBoard (MQTT, device access token) → dashboard.

- Firmware: PlatformIO, Arduino framework, env `esp32-s3-devkitc-1`. Build with `pio run` (or `python -m platformio run`).
- `include/secrets.h` is git-ignored; copy from `include/secrets.example.h`. Never commit real credentials.
- Pins/timing/calibration constants live in `include/config.h`. Sensor Vo must stay on an ADC1 pin (ADC2 is unusable with Wi-Fi).
- Telemetry keys (`pm25`, `pm25Avg`, `aqi`, `aqiCategory`, `dustVoltage`, `dustZeroVoltage`, `samples`, `rssi`) are shared by `src/main.cpp`, `tools/simulate.py`, and `docs/dashboard.md` — keep them in sync.
- AQI uses US EPA 2024 PM2.5 breakpoints; `src/aqi.cpp` and `tools/simulate.py` duplicate the table.
- Flashing and serial monitoring need the physical board, so they only work on the user's machine.
