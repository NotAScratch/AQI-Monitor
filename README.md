# AQI-Monitor

Live air-quality monitor: an **ESP32-S3** reads a **Sharp GP2Y1010AU0F** optical dust sensor, converts it to an estimated PM2.5 concentration and US EPA AQI, and streams it to a **ThingsBoard** dashboard over MQTT.

```
GP2Y1010AU0F --(pulsed IR, analog Vo)--> ESP32-S3 --(Wi-Fi, MQTT)--> ThingsBoard --> Dashboard
```

## Hardware

| Part | Notes |
|---|---|
| ESP32-S3 dev board | e.g. ESP32-S3-DevKitC-1 |
| Sharp GP2Y1010AU0F | 6-pin dust sensor, runs on 5 V |
| 150 Ω resistor + 220 µF capacitor | LED supply filter (from the datasheet) |
| 10 kΩ + 20 kΩ resistors | Divider so the 5 V sensor output can't over-drive the 3.3 V ADC |

### Wiring

| GP2Y10 pin | Connect to |
|---|---|
| 1 V-LED | 5 V through **150 Ω**; also **220 µF** from pin 1 to GND |
| 2 LED-GND | GND |
| 3 LED | ESP32 **GPIO5** (LOW turns the IR LED on) |
| 4 S-GND | GND |
| 5 Vo | **10 kΩ** → ESP32 **GPIO4**, and **20 kΩ** from GPIO4 to GND |
| 6 Vcc | 5 V |

```
 5V ──150Ω──┬── pin1 V-LED            pin5 Vo ──10k──┬── GPIO4 (ADC1_CH3)
            ═ 220µF                                  20k
 GND ───────┴── pin2, pin4            GND ───────────┘
 GPIO5 ──────── pin3 LED              5V  ────────── pin6 Vcc
```

Keep the sensor output on an **ADC1 pin (GPIO1–10)**: ADC2 can't be read while Wi-Fi is on. Pins and resistor values live in [`AQI_Monitor/config.h`](AQI_Monitor/config.h).

## Firmware

The sketch is in [`AQI_Monitor/`](AQI_Monitor). First copy `AQI_Monitor/secrets.example.h` to `AQI_Monitor/secrets.h` and fill in your Wi-Fi details and ThingsBoard token.

**Arduino IDE**
1. Install the ESP32 boards: **Boards Manager** → "esp32 by Espressif Systems".
2. Install the libraries: **Library Manager** → **PubSubClient** (Nick O'Leary) and **ArduinoJson** (Benoit Blanchon, v7).
3. Open `AQI_Monitor/AQI_Monitor.ino`, select **ESP32S3 Dev Module** and your port, then click **Upload**.
4. Open **Serial Monitor** at 115200 baud. If you see no output and are on the native USB port, set **Tools → USB CDC On Boot → Enabled**.

**PlatformIO** (VS Code extension or `pip install platformio`)
```sh
pio run -t upload
pio device monitor
```

How it works:

1. Every 10 ms the IR LED is pulsed for 0.32 ms and the output sampled at 0.28 ms (datasheet timing).
2. Every 10 s (≈1000 pulses) the mean voltage becomes one reading: `pm25 = (V − Voc) / 0.005` µg/m³.
3. A rolling average over the last 30 readings (5 min) gives `pm25Avg`, which is converted to AQI.
4. Readings are published as JSON to `v1/devices/me/telemetry`.

### Telemetry keys

| Key | Meaning |
|---|---|
| `pm25` | Dust density for the last interval, µg/m³ |
| `pm25Avg` | 5-minute rolling average, µg/m³ |
| `aqi` | US EPA AQI from `pm25Avg` (2024 breakpoints) |
| `aqiCategory` | Good / Moderate / … / Hazardous |
| `dustVoltage` | Mean sensor output, V |
| `dustZeroVoltage` | Current clean-air baseline (Voc), V |
| `samples` | Pulses averaged in the interval |
| `rssi` | Wi-Fi signal, dBm |

Client attributes sent on connect: `firmwareVersion`, `sensor`, `board`, `ip`, `mac`.

### Remote settings (ThingsBoard shared attributes)

Set these on the device's **Attributes → Shared attributes** tab; the device picks them up live and on boot.

| Key | Default | Range |
|---|---|---|
| `telemetryIntervalSec` | 10 | 2–3600 |
| `dustZeroVoltage` | 0.60 | 0–3 V |

### Calibration

The GP2Y10's clean-air voltage (Voc) varies a lot between units (≈0.6–1.5 V). To calibrate, run it in clean air for a few minutes, read `dustVoltage`, and set that as `dustZeroVoltage`. The firmware also lowers the baseline automatically whenever it sees a cleaner reading than the current one.

> The GP2Y10 is a low-cost sensor that doesn't separate particle sizes, so `pm25` is an **estimate**. Official AQI uses 24-hour averages; the value here is a short-term indicator. Treat it as relative and trend data, not regulatory-grade.

## ThingsBoard setup

1. Sign in to your ThingsBoard (e.g. [thingsboard.cloud](https://thingsboard.cloud)).
2. **Entities → Devices → +** add a device, e.g. `AQI Monitor 1`.
3. Open it, **Copy access token**, put it in `AQI_Monitor/secrets.h` as `TB_ACCESS_TOKEN`.
4. Flash the board, then confirm data under the device's **Latest telemetry** tab.
5. Build the dashboard. See [docs/dashboard.md](docs/dashboard.md).

### Test without hardware

```sh
python tools/simulate.py --host https://thingsboard.cloud --token YOUR_TOKEN
```

Sends realistic fake readings with the same keys, so you can build the dashboard first.

## Project layout

```
AQI_Monitor/AQI_Monitor.ino     main sketch: Wi-Fi, MQTT, telemetry loop
AQI_Monitor/config.h            pins, timing, calibration, intervals
AQI_Monitor/secrets.example.h   template for Wi-Fi + ThingsBoard credentials
AQI_Monitor/dust_sensor.*       GP2Y10 pulse sampling and conversion
AQI_Monitor/aqi.*               PM2.5 -> US EPA AQI
platformio.ini                  optional PlatformIO build of the same sketch
tools/simulate.py               fake-data publisher for dashboard testing
docs/dashboard.md               ThingsBoard dashboard guide
```
