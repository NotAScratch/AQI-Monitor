# AQI-Monitor

Live air-quality monitor: an **ESP32** reads a **Sharp GP2Y1010AU0F** optical dust sensor, converts it to an estimated PM2.5 concentration and AQI (US EPA or India CPCB), scrolls it on a **P10 LED panel**, and streams it to a **ThingsBoard** dashboard over MQTT.

```
GP2Y1010AU0F --(pulsed IR, analog Vo)--> ESP32 --(Wi-Fi, MQTT)--> ThingsBoard --> Dashboard
                                            \--(SPI)--> P10 LED panel
```

## Hardware

| Part | Notes |
|---|---|
| ESP32 dev board (classic ESP32) | e.g. ESP32 DevKit V1 / WROOM-32. Not ESP32-S3: DMD32Plus needs VSPI |
| P10 LED panel (32×16, HUB12) | Single-colour, with its own 5 V supply |
| Sharp GP2Y1010AU0F | 6-pin dust sensor, runs on 5 V |
| 150 Ω resistor + 220 µF capacitor | LED supply filter (from the datasheet) |
| 10 kΩ + 20 kΩ resistors | Divider so the 5 V sensor output can't over-drive the 3.3 V ADC |

### Wiring

| GP2Y10 pin | Connect to |
|---|---|
| 1 V-LED | 5 V through **150 Ω**; also **220 µF** from pin 1 to GND |
| 2 LED-GND | GND |
| 3 LED | ESP32 **GPIO25** (LOW turns the IR LED on) |
| 4 S-GND | GND |
| 5 Vo | **10 kΩ** → ESP32 **GPIO34**, and **20 kΩ** from GPIO34 to GND |
| 6 Vcc | 5 V |

```
 5V ──150Ω──┬── pin1 V-LED            pin5 Vo ──10k──┬── GPIO34 (ADC1_CH6)
            ═ 220µF                                  20k
 GND ───────┴── pin2, pin4            GND ───────────┘
 GPIO25 ─────── pin3 LED              5V  ────────── pin6 Vcc
```

Keep the sensor output on an **ADC1 pin (GPIO32–39)**: ADC2 can't be read while Wi-Fi is on. Pins and resistor values live in [`AQI_Monitor/config.h`](AQI_Monitor/config.h).

| P10 HUB12 pin | ESP32 |
|---|---|
| OE | GPIO22 |
| A | GPIO19 |
| B | GPIO21 |
| CLK | GPIO18 (VSPI SCK) |
| SCLK / LAT | GPIO2 |
| R (data) | GPIO23 (VSPI MOSI) |
| GND | GND (common with the panel's 5 V supply) |

Leave **GPIO5** unconnected: the display library skips refreshes while it reads LOW, so the firmware pulls it up. Power the panel from its own 5 V supply, not the ESP32's 5 V pin.

## Firmware

The sketch is in [`AQI_Monitor/`](AQI_Monitor). First copy `AQI_Monitor/secrets.example.h` to `AQI_Monitor/secrets.h` and fill in your Wi-Fi details and ThingsBoard token.

**Arduino IDE**
1. Install the ESP32 boards: **Boards Manager** → "esp32 by Espressif Systems".
2. Install the libraries: **Library Manager** → **PubSubClient** (Nick O'Leary), **ArduinoJson** (Benoit Blanchon, v7) and **DMD32Plus** (Ahmad Fathan).
3. Open `AQI_Monitor/AQI_Monitor.ino`, select **ESP32 Dev Module** and your port, then click **Upload**.
4. Open **Serial Monitor** at 115200 baud.

**PlatformIO** (VS Code extension or `pip install platformio`)
```sh
pio run -t upload
pio device monitor
```

How it works:

1. Every 10 ms the IR LED is pulsed for 0.32 ms and the output sampled at 0.28 ms (datasheet timing). This runs in its own high-priority task so the display and MQTT can't shift the sample point.
2. Every 10 s (≈1000 pulses) the mean voltage becomes one reading: `pm25 = (V − Voc) / 0.005` µg/m³.
3. A rolling average over the last 30 readings (5 min) gives `pm25Avg`, which is converted to AQI. The scale is set by `AQI_STANDARD` in `config.h`: `1` = US EPA (2024 breakpoints), `2` = India CPCB.
4. Readings are published as JSON to `v1/devices/me/telemetry`, and the panel scrolls `AQI: <value>  <category>`.

### Telemetry keys

| Key | Meaning |
|---|---|
| `pm25` | Dust density for the last interval, µg/m³ |
| `pm25Avg` | 5-minute rolling average, µg/m³ |
| `aqi` | AQI from `pm25Avg` (US EPA 2024 or India CPCB, per `AQI_STANDARD`) |
| `aqiCategory` | EPA: Good … Hazardous; CPCB: Good … Severe |
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
| `dustZeroVoltage` | 0.55 | 0–3 V |

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
AQI_Monitor/aqi.*               PM2.5 -> AQI (US EPA or India CPCB)
AQI_Monitor/display.*           P10 LED panel refresh and scrolling text
platformio.ini                  optional PlatformIO build of the same sketch
tools/simulate.py               fake-data publisher for dashboard testing
docs/dashboard.md               ThingsBoard dashboard guide
```
