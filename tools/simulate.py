#!/usr/bin/env python3
"""Send fake AQI telemetry to ThingsBoard so you can build the dashboard
before the hardware is wired up. Uses only the Python standard library.

    python tools/simulate.py --host https://thingsboard.cloud --token YOUR_TOKEN
"""

import argparse
import json
import math
import random
import time
import urllib.request

# US EPA PM2.5 breakpoints (2024), same as src/aqi.cpp
BREAKPOINTS = [
    (0.0, 9.0, 0, 50),
    (9.1, 35.4, 51, 100),
    (35.5, 55.4, 101, 150),
    (55.5, 125.4, 151, 200),
    (125.5, 225.4, 201, 300),
    (225.5, 325.4, 301, 500),
]
CATEGORIES = [
    (50, "Good"),
    (100, "Moderate"),
    (150, "Unhealthy for Sensitive Groups"),
    (200, "Unhealthy"),
    (300, "Very Unhealthy"),
    (500, "Hazardous"),
]


def aqi_from_pm25(pm25: float) -> int:
    c = math.floor(max(pm25, 0) * 10) / 10
    for c_lo, c_hi, i_lo, i_hi in BREAKPOINTS:
        if c <= c_hi:
            c = max(c, c_lo)
            return round((i_hi - i_lo) / (c_hi - c_lo) * (c - c_lo) + i_lo)
    return 500


def category(aqi: int) -> str:
    return next(name for limit, name in CATEGORIES if aqi <= limit)


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--host", default="https://thingsboard.cloud")
    p.add_argument("--token", required=True, help="device access token")
    p.add_argument("--interval", type=float, default=10, help="seconds between messages")
    args = p.parse_args()

    url = f"{args.host.rstrip('/')}/api/v1/{args.token}/telemetry"
    zero_v, history, t0 = 0.6, [], time.time()

    while True:
        # Slow daily-ish wave plus noise, 5-150 ug/m3
        t = time.time() - t0
        pm25 = max(0.0, 60 + 55 * math.sin(t / 600) + random.gauss(0, 6))
        history = (history + [pm25])[-30:]
        pm25_avg = sum(history) / len(history)
        aqi = aqi_from_pm25(pm25_avg)
        payload = {
            "dustVoltage": round(zero_v + pm25 * 0.005, 3),
            "dustZeroVoltage": zero_v,
            "pm25": round(pm25, 1),
            "pm25Avg": round(pm25_avg, 1),
            "aqi": aqi,
            "aqiCategory": category(aqi),
            "samples": 1000,
            "rssi": random.randint(-75, -50),
        }
        req = urllib.request.Request(
            url,
            data=json.dumps(payload).encode(),
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urllib.request.urlopen(req, timeout=10) as resp:
            print(resp.status, payload)
        time.sleep(args.interval)


if __name__ == "__main__":
    main()
