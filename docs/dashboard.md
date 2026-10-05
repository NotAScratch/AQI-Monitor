# ThingsBoard dashboard

Widget names below are from ThingsBoard 3.x/4.x; they may differ slightly between versions.

## 1. Create the dashboard

**Dashboards → + → Create new dashboard**, name it `Live AQI`, open it and click **Edit**.

Add an **entity alias** (top-right **Entity aliases** icon):
- Name: `AQI Device`
- Filter type: **Single entity** → Device → your device

## 2. Widgets

| Widget | Bundle → widget | Data key(s) | Settings |
|---|---|---|---|
| Current AQI | Indoor/Outdoor environment → **Air quality index card** (or Cards → Value card) | `aqi` | Title "AQI" |
| Category | Cards → **Label/Value card** or Entities table | `aqiCategory` | |
| PM2.5 now | Cards → **Value card** | `pm25Avg` | Units `µg/m³`, 1 decimal |
| PM2.5 gauge | Analog/Digital gauges → **Radial gauge** | `pm25Avg` | Min 0, max 250 |
| PM2.5 history | Charts → **Time series chart** | `pm25`, `pm25Avg` | Timewindow: last 24 h, aggregation Average 5 min |
| AQI history | Charts → **Time series chart** (bar) | `aqi` | Last 7 days, aggregation Max 1 h |
| Device health | Cards → **Entities table** or Attributes card | `rssi`, `dustVoltage`, `dustZeroVoltage`, `firmwareVersion` | |
| Calibration | Control widgets → **Update shared attributes** (or Input widgets → *Update shared double/integer attribute*) | `dustZeroVoltage`, `telemetryIntervalSec` | Lets you calibrate from the dashboard |

## 3. AQI colours

On the AQI value/gauge widget use these thresholds (US EPA colours):

| AQI | Colour | Category |
|---|---|---|
| 0–50 | `#00E400` | Good |
| 51–100 | `#FFFF00` | Moderate |
| 101–150 | `#FF7E00` | Unhealthy for Sensitive Groups |
| 151–200 | `#FF0000` | Unhealthy |
| 201–300 | `#8F3F97` | Very Unhealthy |
| 301–500 | `#7E0023` | Hazardous |

## 4. Optional: alarms

**Device profiles → your profile → Alarm rules → Add**:
- Alarm type `High AQI`, condition `aqi > 150` with duration 10 min, severity *Major*
- Clear condition `aqi <= 100`

Add an **Alarms table** widget to see them on the dashboard, and configure notifications under **Notification center** if you want email/Telegram alerts.

## 5. Optional: device offline alerts

Add a **server attribute** `inactivityTimeout` = `120000` (ms) on the device so it shows as inactive after 2 minutes without data.
