#pragma once

// Copy this file to include/secrets.h and fill in your values.
// secrets.h is git-ignored so your credentials never get pushed.

#define WIFI_SSID      "your-wifi-ssid"
#define WIFI_PASSWORD  "your-wifi-password"

// ThingsBoard server. Use "thingsboard.cloud", "demo.thingsboard.io",
// or the hostname/IP of your self-hosted instance.
#define TB_HOST        "thingsboard.cloud"
#define TB_PORT        1883

// Device access token: ThingsBoard -> Entities -> Devices -> your device
// -> "Copy access token".
#define TB_ACCESS_TOKEN "YOUR_DEVICE_ACCESS_TOKEN"
